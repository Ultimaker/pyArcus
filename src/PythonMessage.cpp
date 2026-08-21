// Copyright (c) 2022 Ultimaker B.V.
// pyArcus is released under the terms of the LGPLv3 or higher.

#include "pyArcus/PythonMessage.h"

#include <Python.h>

#include <cstdint>

#include <google/protobuf/message.h>
#include <google/protobuf/reflection.h>

using namespace Arcus;
using namespace google::protobuf;

PythonMessage::PythonMessage(google::protobuf::Message* message)
{
    _message = message;
    _reflection = message->GetReflection();
    _descriptor = message->GetDescriptor();
}

Arcus::PythonMessage::PythonMessage(const MessagePtr& message)
{
    _shared_message = message;
    _message = message.get();
    _reflection = message->GetReflection();
    _descriptor = message->GetDescriptor();
}

PythonMessage::~PythonMessage()
{
}

std::string Arcus::PythonMessage::getTypeName() const
{
    return std::string(_message->GetTypeName());
}

MessagePtr Arcus::PythonMessage::getSharedMessage() const
{
    return _shared_message;
}

// Instead of `_descriptor->FindFieldByName(field_name)`.
const google::protobuf::FieldDescriptor* findFieldByNameHack(const google::protobuf::Descriptor* _descriptor, const std::string_view field_name)
{
    for (int ii = 0; ii < _descriptor->field_count(); ++ii)
    {
        auto candidate = _descriptor->field(ii);
        if (field_name.compare(candidate->name()) == 0)
        {
            return candidate;
        }
    }
    return nullptr;
}

bool Arcus::PythonMessage::__hasattr__(const std::string& field_name) const
{
    auto field = findFieldByNameHack(_descriptor, field_name);
    return bool(field);
}

// Read one scalar value from a field and return it as a Python object.
// Pass index >= 0 for a repeated field element, or index < 0 for a singular field.
static PyObject* getScalarField(const google::protobuf::Reflection* reflection,
                                const google::protobuf::Message& message,
                                const google::protobuf::FieldDescriptor* field,
                                int index)
{
    const bool repeated = index >= 0;
    switch (field->type())
    {
    case FieldDescriptor::TYPE_FLOAT:
        return PyFloat_FromDouble(repeated ? reflection->GetRepeatedFloat(message, field, index)
                                           : reflection->GetFloat(message, field));
    case FieldDescriptor::TYPE_DOUBLE:
        return PyFloat_FromDouble(repeated ? reflection->GetRepeatedDouble(message, field, index)
                                           : reflection->GetDouble(message, field));
    case FieldDescriptor::TYPE_INT32:
    case FieldDescriptor::TYPE_FIXED32:
    case FieldDescriptor::TYPE_SINT32:
    case FieldDescriptor::TYPE_SFIXED32:
        return PyLong_FromLong(repeated ? reflection->GetRepeatedInt32(message, field, index)
                                        : reflection->GetInt32(message, field));
    case FieldDescriptor::TYPE_INT64:
    case FieldDescriptor::TYPE_FIXED64:
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_SFIXED64:
        return PyLong_FromLongLong(repeated ? reflection->GetRepeatedInt64(message, field, index)
                                            : reflection->GetInt64(message, field));
    case FieldDescriptor::TYPE_UINT32:
        return PyLong_FromUnsignedLong(repeated ? reflection->GetRepeatedUInt32(message, field, index)
                                                : reflection->GetUInt32(message, field));
    case FieldDescriptor::TYPE_UINT64:
        return PyLong_FromUnsignedLongLong(repeated ? reflection->GetRepeatedUInt64(message, field, index)
                                                    : reflection->GetUInt64(message, field));
    case FieldDescriptor::TYPE_BOOL:
    {
        bool v = repeated ? reflection->GetRepeatedBool(message, field, index) : reflection->GetBool(message, field);
        if (v)
        {
            Py_RETURN_TRUE;
        }
        Py_RETURN_FALSE;
    }
    case FieldDescriptor::TYPE_BYTES:
    {
        std::string data = repeated ? reflection->GetRepeatedString(message, field, index)
                                    : reflection->GetString(message, field);
        return PyBytes_FromStringAndSize(data.c_str(), data.size());
    }
    case FieldDescriptor::TYPE_STRING:
    {
        std::string data = repeated ? reflection->GetRepeatedString(message, field, index)
                                    : reflection->GetString(message, field);
        return PyUnicode_FromString(data.c_str());
    }
    case FieldDescriptor::TYPE_ENUM:
        return PyLong_FromLong(repeated ? reflection->GetRepeatedEnumValue(message, field, index)
                                        : reflection->GetEnumValue(message, field));
    default:
        PyErr_SetString(PyExc_ValueError, "Could not handle value of field");
        return nullptr;
    }
}

PyObject* Arcus::PythonMessage::__getattr__(const std::string& field_name) const
{
    auto field = findFieldByNameHack(_descriptor, field_name);
    if (! field)
    {
        PyErr_SetString(PyExc_AttributeError, field_name.c_str());
        return nullptr;
    }

    if (field->is_repeated())
    {
        int count = _reflection->FieldSize(*_message, field);
        PyObject* list = PyList_New(count);
        if (! list)
        {
            return nullptr;
        }
        for (int i = 0; i < count; ++i)
        {
            PyObject* item = getScalarField(_reflection, *_message, field, i);
            if (! item)
            {
                Py_DECREF(list);
                return nullptr;
            }
            PyList_SET_ITEM(list, i, item); // steals reference
        }
        return list;
    }

    return getScalarField(_reflection, *_message, field, -1);
}

// Write one scalar Python value into a field.
// Pass add = true to append to a repeated field, or add = false to set a singular field.
static bool setScalarField(google::protobuf::Message* message,
                           const google::protobuf::Reflection* reflection,
                           const google::protobuf::FieldDescriptor* field,
                           PyObject* value,
                           bool add)
{
    switch (field->type())
    {
    case FieldDescriptor::TYPE_FLOAT:
    {
        double v = PyFloat_AsDouble(value);
        if (v == -1.0 && PyErr_Occurred())
        {
            return false;
        }
        if (add)
        {
            reflection->AddFloat(message, field, static_cast<float>(v));
        }
        else
        {
            reflection->SetFloat(message, field, static_cast<float>(v));
        }
        break;
    }
    case FieldDescriptor::TYPE_DOUBLE:
    {
        double v = PyFloat_AsDouble(value);
        if (v == -1.0 && PyErr_Occurred())
        {
            return false;
        }
        add ? reflection->AddDouble(message, field, v) : reflection->SetDouble(message, field, v);
        break;
    }
    case FieldDescriptor::TYPE_INT32:
    case FieldDescriptor::TYPE_FIXED32:
    case FieldDescriptor::TYPE_SINT32:
    case FieldDescriptor::TYPE_SFIXED32:
    {
        long v = PyLong_AsLong(value);
        if (v == -1 && PyErr_Occurred())
        {
            return false;
        }
        if (add)
        {
            reflection->AddInt32(message, field, static_cast<int32_t>(v));
        }
        else
        {
            reflection->SetInt32(message, field, static_cast<int32_t>(v));
        }
        break;
    }
    case FieldDescriptor::TYPE_INT64:
    case FieldDescriptor::TYPE_FIXED64:
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_SFIXED64:
    {
        long long v = PyLong_AsLongLong(value);
        if (v == -1 && PyErr_Occurred())
        {
            return false;
        }
        if (add)
        {
            reflection->AddInt64(message, field, static_cast<int64_t>(v));
        }
        else
        {
            reflection->SetInt64(message, field, static_cast<int64_t>(v));
        }
        break;
    }
    case FieldDescriptor::TYPE_UINT32:
    {
        unsigned long v = PyLong_AsUnsignedLong(value);
        if (v == static_cast<unsigned long>(-1) && PyErr_Occurred())
        {
            return false;
        }
        if (add)
        {
            reflection->AddUInt32(message, field, static_cast<uint32_t>(v));
        }
        else
        {
            reflection->SetUInt32(message, field, static_cast<uint32_t>(v));
        }
        break;
    }
    case FieldDescriptor::TYPE_UINT64:
    {
        unsigned long long v = PyLong_AsUnsignedLongLong(value);
        if (v == static_cast<unsigned long long>(-1) && PyErr_Occurred())
        {
            return false;
        }
        if (add)
        {
            reflection->AddUInt64(message, field, static_cast<uint64_t>(v));
        }
        else
        {
            reflection->SetUInt64(message, field, static_cast<uint64_t>(v));
        }
        break;
    }
    case FieldDescriptor::TYPE_BOOL:
    {
        int v = PyObject_IsTrue(value);
        if (v < 0)
        {
            return false;
        }
        add ? reflection->AddBool(message, field, v != 0) : reflection->SetBool(message, field, v != 0);
        break;
    }
    case FieldDescriptor::TYPE_BYTES:
    {
        Py_buffer buffer;
        if (PyObject_GetBuffer(value, &buffer, PyBUF_SIMPLE) < 0)
        {
            return false;
        }
        std::string str(reinterpret_cast<char*>(buffer.buf), buffer.len);
        PyBuffer_Release(&buffer);
        add ? reflection->AddString(message, field, str) : reflection->SetString(message, field, str);
        break;
    }
    case FieldDescriptor::TYPE_STRING:
    {
        const char* str = PyUnicode_AsUTF8(value);
        if (! str)
        {
            return false;
        }
        add ? reflection->AddString(message, field, str) : reflection->SetString(message, field, str);
        break;
    }
    case FieldDescriptor::TYPE_ENUM:
    {
        if (PyUnicode_Check(value))
        {
            const char* name = PyUnicode_AsUTF8(value);
            if (! name)
            {
                return false;
            }
            auto enum_value = field->enum_type()->FindValueByName(name);
            if (! enum_value)
            {
                PyErr_Format(PyExc_ValueError, "Unknown enum value: %s", name);
                return false;
            }
            add ? reflection->AddEnum(message, field, enum_value) : reflection->SetEnum(message, field, enum_value);
        }
        else
        {
            long v = PyLong_AsLong(value);
            if (v == -1 && PyErr_Occurred())
            {
                return false;
            }
            if (add)
            {
                reflection->AddEnumValue(message, field, static_cast<int>(v));
            }
            else
            {
                reflection->SetEnumValue(message, field, static_cast<int>(v));
            }
        }
        break;
    }
    default:
        PyErr_SetString(PyExc_ValueError, "Could not handle value of field");
        return false;
    }
    return true;
}

void Arcus::PythonMessage::__setattr__(const std::string& field_name, PyObject* value)
{
    auto field = findFieldByNameHack(_descriptor, field_name);
    if (! field)
    {
        PyErr_SetString(PyExc_AttributeError, field_name.c_str());
        return;
    }

    if (field->is_repeated())
    {
        PyObject* iter = PyObject_GetIter(value);
        if (! iter)
        {
            return;
        }
        _reflection->ClearField(_message, field);
        PyObject* item;
        while ((item = PyIter_Next(iter)) != nullptr)
        {
            bool ok = setScalarField(_message, _reflection, field, item, true);
            Py_DECREF(item);
            if (! ok)
            {
                Py_DECREF(iter);
                return;
            }
        }
        Py_DECREF(iter);
        if (PyErr_Occurred())
        {
            return;
        }
        return;
    }

    setScalarField(_message, _reflection, field, value, false);
}

PythonMessage* Arcus::PythonMessage::addRepeatedMessage(const std::string& field_name)
{
    auto field = findFieldByNameHack(_descriptor, field_name);
    if (! field)
    {
        PyErr_SetString(PyExc_AttributeError, field_name.c_str());
        return nullptr;
    }

    Message* message = _reflection->AddMessage(_message, field);
    return new PythonMessage(message);
}

int PythonMessage::repeatedMessageCount(const std::string& field_name) const
{
    auto field = findFieldByNameHack(_descriptor, field_name);
    if (! field)
    {
        PyErr_SetString(PyExc_AttributeError, field_name.c_str());
        return -1;
    }

    return _reflection->FieldSize(*_message, field);
}

PythonMessage* Arcus::PythonMessage::getMessage(const std::string& field_name)
{
    auto field = findFieldByNameHack(_descriptor, field_name);
    if (! field)
    {
        PyErr_SetString(PyExc_AttributeError, field_name.c_str());
        return nullptr;
    }
    return new PythonMessage(_reflection->MutableMessage(_message, field));
}

PythonMessage* Arcus::PythonMessage::getRepeatedMessage(const std::string& field_name, int index)
{
    auto field = findFieldByNameHack(_descriptor, field_name);
    if (! field)
    {
        PyErr_SetString(PyExc_AttributeError, field_name.c_str());
        return nullptr;
    }

    if (index < 0 || index > _reflection->FieldSize(*_message, field))
    {
        PyErr_SetString(PyExc_IndexError, field_name.c_str());
        return nullptr;
    }

    return new PythonMessage(_reflection->MutableRepeatedMessage(_message, field, index));
}

int Arcus::PythonMessage::getEnumValue(const std::string& enum_value) const
{
    auto field = _descriptor->FindEnumValueByName(enum_value);
    if (! field)
    {
        return -1;
    }

    return field->number();
}

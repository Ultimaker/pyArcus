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

// Helper: convert a single scalar value at a given repeated-field index to a PyObject.
static PyObject* getRepeatedScalar(const google::protobuf::Reflection* reflection,
                                   const google::protobuf::Message& message,
                                   const google::protobuf::FieldDescriptor* field,
                                   int index)
{
    switch (field->type())
    {
    case FieldDescriptor::TYPE_FLOAT:
        return PyFloat_FromDouble(reflection->GetRepeatedFloat(message, field, index));
    case FieldDescriptor::TYPE_DOUBLE:
        return PyFloat_FromDouble(reflection->GetRepeatedDouble(message, field, index));
    case FieldDescriptor::TYPE_INT32:
    case FieldDescriptor::TYPE_FIXED32:
    case FieldDescriptor::TYPE_SINT32:
    case FieldDescriptor::TYPE_SFIXED32:
        return PyLong_FromLong(reflection->GetRepeatedInt32(message, field, index));
    case FieldDescriptor::TYPE_INT64:
    case FieldDescriptor::TYPE_FIXED64:
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_SFIXED64:
        return PyLong_FromLongLong(reflection->GetRepeatedInt64(message, field, index));
    case FieldDescriptor::TYPE_UINT32:
        return PyLong_FromUnsignedLong(reflection->GetRepeatedUInt32(message, field, index));
    case FieldDescriptor::TYPE_UINT64:
        return PyLong_FromUnsignedLongLong(reflection->GetRepeatedUInt64(message, field, index));
    case FieldDescriptor::TYPE_BOOL:
        if (reflection->GetRepeatedBool(message, field, index))
        {
            Py_RETURN_TRUE;
        }
        else
        {
            Py_RETURN_FALSE;
        }
    case FieldDescriptor::TYPE_BYTES:
    {
        std::string data = reflection->GetRepeatedString(message, field, index);
        return PyBytes_FromStringAndSize(data.c_str(), data.size());
    }
    case FieldDescriptor::TYPE_STRING:
        return PyUnicode_FromString(reflection->GetRepeatedString(message, field, index).c_str());
    case FieldDescriptor::TYPE_ENUM:
        return PyLong_FromLong(reflection->GetRepeatedEnumValue(message, field, index));
    default:
        PyErr_SetString(PyExc_ValueError, "Could not handle value of repeated field");
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
            PyObject* item = getRepeatedScalar(_reflection, *_message, field, i);
            if (! item)
            {
                Py_DECREF(list);
                return nullptr;
            }
            PyList_SET_ITEM(list, i, item); // steals reference
        }
        return list;
    }

    switch (field->type())
    {
    case FieldDescriptor::TYPE_FLOAT:
        return PyFloat_FromDouble(_reflection->GetFloat(*_message, field));
    case FieldDescriptor::TYPE_DOUBLE:
        return PyFloat_FromDouble(_reflection->GetDouble(*_message, field));
    case FieldDescriptor::TYPE_INT32:
    case FieldDescriptor::TYPE_FIXED32:
    case FieldDescriptor::TYPE_SINT32:
    case FieldDescriptor::TYPE_SFIXED32:
        return PyLong_FromLong(_reflection->GetInt32(*_message, field));
    case FieldDescriptor::TYPE_INT64:
    case FieldDescriptor::TYPE_FIXED64:
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_SFIXED64:
        return PyLong_FromLongLong(_reflection->GetInt64(*_message, field));
    case FieldDescriptor::TYPE_UINT32:
        return PyLong_FromUnsignedLong(_reflection->GetUInt32(*_message, field));
    case FieldDescriptor::TYPE_UINT64:
        return PyLong_FromUnsignedLongLong(_reflection->GetUInt64(*_message, field));
    case FieldDescriptor::TYPE_BOOL:
        if (_reflection->GetBool(*_message, field))
        {
            Py_RETURN_TRUE;
        }
        else
        {
            Py_RETURN_FALSE;
        }
    case FieldDescriptor::TYPE_BYTES:
    {
        std::string data = _reflection->GetString(*_message, field);
        return PyBytes_FromStringAndSize(data.c_str(), data.size());
    }
    case FieldDescriptor::TYPE_STRING:
        return PyUnicode_FromString(_reflection->GetString(*_message, field).c_str());
    case FieldDescriptor::TYPE_ENUM:
        return PyLong_FromLong(_reflection->GetEnumValue(*_message, field));
    default:
        PyErr_SetString(PyExc_ValueError, "Could not handle value of field");
        return nullptr;
    }
}

// Helper: append a single Python scalar value to a repeated field.
static bool addRepeatedScalar(google::protobuf::Message* message,
                               const google::protobuf::Reflection* reflection,
                               const google::protobuf::FieldDescriptor* field,
                               PyObject* item)
{
    switch (field->type())
    {
    case FieldDescriptor::TYPE_FLOAT:
    {
        double v = PyFloat_AsDouble(item);
        if (v == -1.0 && PyErr_Occurred())
        {
            return false;
        }
        reflection->AddFloat(message, field, static_cast<float>(v));
        break;
    }
    case FieldDescriptor::TYPE_DOUBLE:
    {
        double v = PyFloat_AsDouble(item);
        if (v == -1.0 && PyErr_Occurred())
        {
            return false;
        }
        reflection->AddDouble(message, field, v);
        break;
    }
    case FieldDescriptor::TYPE_INT32:
    case FieldDescriptor::TYPE_SFIXED32:
    case FieldDescriptor::TYPE_FIXED32:
    case FieldDescriptor::TYPE_SINT32:
    {
        long v = PyLong_AsLong(item);
        if (v == -1 && PyErr_Occurred())
        {
            return false;
        }
        reflection->AddInt32(message, field, static_cast<int32_t>(v));
        break;
    }
    case FieldDescriptor::TYPE_INT64:
    case FieldDescriptor::TYPE_FIXED64:
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_SFIXED64:
    {
        long long v = PyLong_AsLongLong(item);
        if (v == -1 && PyErr_Occurred())
        {
            return false;
        }
        reflection->AddInt64(message, field, static_cast<int64_t>(v));
        break;
    }
    case FieldDescriptor::TYPE_UINT32:
    {
        unsigned long v = PyLong_AsUnsignedLong(item);
        if (v == static_cast<unsigned long>(-1) && PyErr_Occurred())
        {
            return false;
        }
        reflection->AddUInt32(message, field, static_cast<uint32_t>(v));
        break;
    }
    case FieldDescriptor::TYPE_UINT64:
    {
        unsigned long long v = PyLong_AsUnsignedLongLong(item);
        if (v == static_cast<unsigned long long>(-1) && PyErr_Occurred())
        {
            return false;
        }
        reflection->AddUInt64(message, field, static_cast<uint64_t>(v));
        break;
    }
    case FieldDescriptor::TYPE_BOOL:
        reflection->AddBool(message, field, item == Py_True);
        break;
    case FieldDescriptor::TYPE_BYTES:
    {
        Py_buffer buffer;
        if (PyObject_GetBuffer(item, &buffer, PyBUF_SIMPLE) < 0)
        {
            return false;
        }
        std::string str(reinterpret_cast<char*>(buffer.buf), buffer.len);
        PyBuffer_Release(&buffer);
        reflection->AddString(message, field, str);
        break;
    }
    case FieldDescriptor::TYPE_STRING:
    {
        const char* str = PyUnicode_AsUTF8(item);
        if (! str)
        {
            return false;
        }
        reflection->AddString(message, field, str);
        break;
    }
    case FieldDescriptor::TYPE_ENUM:
    {
        if (PyUnicode_Check(item))
        {
            const char* name = PyUnicode_AsUTF8(item);
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
            reflection->AddEnum(message, field, enum_value);
        }
        else
        {
            long v = PyLong_AsLong(item);
            if (v == -1 && PyErr_Occurred())
            {
                return false;
            }
            reflection->AddEnumValue(message, field, static_cast<int>(v));
        }
        break;
    }
    default:
        PyErr_SetString(PyExc_ValueError, "Could not handle value of repeated field");
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
            bool ok = addRepeatedScalar(_message, _reflection, field, item);
            Py_DECREF(item);
            if (! ok)
            {
                Py_DECREF(iter);
                return;
            }
        }
        Py_DECREF(iter);
        return;
    }

    switch (field->type())
    {
    case FieldDescriptor::TYPE_FLOAT:
        _reflection->SetFloat(_message, field, PyFloat_AsDouble(value));
        break;
    case FieldDescriptor::TYPE_DOUBLE:
        _reflection->SetDouble(_message, field, PyFloat_AsDouble(value));
        break;
    case FieldDescriptor::TYPE_INT32:
    case FieldDescriptor::TYPE_SFIXED32:
    case FieldDescriptor::TYPE_FIXED32:
    case FieldDescriptor::TYPE_SINT32:
        _reflection->SetInt32(_message, field, PyLong_AsLong(value));
        break;
    case FieldDescriptor::TYPE_INT64:
    case FieldDescriptor::TYPE_FIXED64:
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_SFIXED64:
        _reflection->SetInt64(_message, field, PyLong_AsLongLong(value));
        break;
    case FieldDescriptor::TYPE_UINT32:
        _reflection->SetUInt32(_message, field, PyLong_AsUnsignedLong(value));
        break;
    case FieldDescriptor::TYPE_UINT64:
        _reflection->SetUInt64(_message, field, PyLong_AsUnsignedLongLong(value));
        break;
    case FieldDescriptor::TYPE_BOOL:
        if (value == Py_True)
        {
            _reflection->SetBool(_message, field, true);
        }
        else
        {
            _reflection->SetBool(_message, field, false);
        }
        break;
    case FieldDescriptor::TYPE_BYTES:
    {
        Py_buffer buffer;
        PyObject_GetBuffer(value, &buffer, PyBUF_SIMPLE);

        std::string str(reinterpret_cast<char*>(buffer.buf), buffer.len);
        _reflection->SetString(_message, field, str);
        break;
    }
    case FieldDescriptor::TYPE_STRING:
        _reflection->SetString(_message, field, PyUnicode_AsUTF8(value));
        break;
    case FieldDescriptor::TYPE_ENUM:
    {
        if (PyUnicode_Check(value))
        {
            auto enum_value = _descriptor->FindEnumValueByName(PyUnicode_AsUTF8(value));
            _reflection->SetEnum(_message, field, enum_value);
        }
        else
        {
            _reflection->SetEnumValue(_message, field, PyLong_AsLong(value));
        }
        break;
    }
    default:
        PyErr_SetString(PyExc_ValueError, "Could not handle value of field");
        break;
    }
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

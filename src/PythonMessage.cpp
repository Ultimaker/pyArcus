// Copyright (c) 2022 Ultimaker B.V.
// pyArcus is released under the terms of the LGPLv3 or higher.

#include "pyArcus/PythonMessage.h"

#include <Python.h>

#include <cstdint>

#include <google/protobuf/message.h>
// #include <google/protobuf/reflection.h>

namespace gp = google::protobuf;

namespace Arcus
{

PythonMessage::PythonMessage(gp::Message* message) : _message(message), _reflection(message->GetReflection()), _descriptor(message->GetDescriptor())
{
}

PythonMessage::PythonMessage(const MessagePtr& message) : _shared_message(message), _message(message.get()), _reflection(message->GetReflection()), _descriptor(message->GetDescriptor())
{
}

PythonMessage::~PythonMessage() = default;

std::string PythonMessage::getTypeName() const
{
    return std::string(_message->GetTypeName());
}

MessagePtr PythonMessage::getSharedMessage() const
{
    return _shared_message;
}

const gp::FieldDescriptor* PythonMessage::findFieldByNameHack(const std::string& field_name) const
{
    for (int ii = 0; ii < _descriptor->field_count(); ++ii)
    {
        const gp::FieldDescriptor* candidate = _descriptor->field(ii);
        if (field_name == candidate->name())
        {
            return candidate;
        }
    }
    return nullptr;
}

bool PythonMessage::__hasattr__(const std::string& field_name) const
{
    const gp::FieldDescriptor* field = findFieldByNameHack(field_name);
    return bool(field);
}

PyObject* PythonMessage::getFieldValue(const gp::FieldDescriptor* field, const std::optional<int>& index) const
{
    const bool repeated = index.has_value();
    switch (field->type())
    {
    case gp::FieldDescriptor::TYPE_FLOAT:
        return PyFloat_FromDouble(repeated ? _reflection->GetRepeatedFloat(*_message, field, *index) : _reflection->GetFloat(*_message, field));
    case gp::FieldDescriptor::TYPE_DOUBLE:
        return PyFloat_FromDouble(repeated ? _reflection->GetRepeatedDouble(*_message, field, *index) : _reflection->GetDouble(*_message, field));
    case gp::FieldDescriptor::TYPE_INT32:
    case gp::FieldDescriptor::TYPE_FIXED32:
    case gp::FieldDescriptor::TYPE_SINT32:
    case gp::FieldDescriptor::TYPE_SFIXED32:
        return PyLong_FromLong(repeated ? _reflection->GetRepeatedInt32(*_message, field, *index) : _reflection->GetInt32(*_message, field));
    case gp::FieldDescriptor::TYPE_INT64:
    case gp::FieldDescriptor::TYPE_FIXED64:
    case gp::FieldDescriptor::TYPE_SINT64:
    case gp::FieldDescriptor::TYPE_SFIXED64:
        return PyLong_FromLongLong(repeated ? _reflection->GetRepeatedInt64(*_message, field, *index) : _reflection->GetInt64(*_message, field));
    case gp::FieldDescriptor::TYPE_UINT32:
        return PyLong_FromUnsignedLong(repeated ? _reflection->GetRepeatedUInt32(*_message, field, *index) : _reflection->GetUInt32(*_message, field));
    case gp::FieldDescriptor::TYPE_UINT64:
        return PyLong_FromUnsignedLongLong(repeated ? _reflection->GetRepeatedUInt64(*_message, field, *index) : _reflection->GetUInt64(*_message, field));
    case gp::FieldDescriptor::TYPE_BOOL:
    {
        const bool value_bool = repeated ? _reflection->GetRepeatedBool(*_message, field, *index) : _reflection->GetBool(*_message, field);
        if (value_bool)
        {
            Py_RETURN_TRUE;
        }
        Py_RETURN_FALSE;
    }
    case gp::FieldDescriptor::TYPE_BYTES:
    {
        const std::string data = repeated ? _reflection->GetRepeatedString(*_message, field, *index) : _reflection->GetString(*_message, field);
        return PyBytes_FromStringAndSize(data.c_str(), data.size());
    }
    case gp::FieldDescriptor::TYPE_STRING:
    {
        const std::string data = repeated ? _reflection->GetRepeatedString(*_message, field, *index) : _reflection->GetString(*_message, field);
        return PyUnicode_FromString(data.c_str());
    }
    case gp::FieldDescriptor::TYPE_ENUM:
        return PyLong_FromLong(repeated ? _reflection->GetRepeatedEnumValue(*_message, field, *index) : _reflection->GetEnumValue(*_message, field));
    default:
        PyErr_SetString(PyExc_ValueError, "Could not handle value of field");
        return nullptr;
    }
}

PyObject* PythonMessage::__getattr__(const std::string& field_name) const
{
    const gp::FieldDescriptor* field = findFieldByNameHack(field_name);
    if (! field)
    {
        PyErr_SetString(PyExc_AttributeError, field_name.c_str());
        return nullptr;
    }

    if (field->is_repeated())
    {
        const int count = _reflection->FieldSize(*_message, field);
        PyObject* list = PyList_New(count);
        if (! list)
        {
            return nullptr;
        }
        for (int i = 0; i < count; ++i)
        {
            PyObject* item = getFieldValue(field, i);
            if (! item)
            {
                Py_DECREF(list);
                return nullptr;
            }
            PyList_SET_ITEM(list, i, item); // steals reference
        }
        return list;
    }

    return getFieldValue(field);
}

bool PythonMessage::setFieldValue(const gp::FieldDescriptor* field, PyObject* value, bool append)
{
    switch (field->type())
    {
    case gp::FieldDescriptor::TYPE_FLOAT:
    {
        double value_double = PyFloat_AsDouble(value);
        if (value_double == -1.0 && PyErr_Occurred())
        {
            return false;
        }
        if (append)
        {
            _reflection->AddFloat(_message, field, static_cast<float>(value_double));
        }
        else
        {
            _reflection->SetFloat(_message, field, static_cast<float>(value_double));
        }
        break;
    }
    case gp::FieldDescriptor::TYPE_DOUBLE:
    {
        double value_double = PyFloat_AsDouble(value);
        if (value_double == -1.0 && PyErr_Occurred())
        {
            return false;
        }
        append ? _reflection->AddDouble(_message, field, value_double) : _reflection->SetDouble(_message, field, value_double);
        break;
    }
    case gp::FieldDescriptor::TYPE_INT32:
    case gp::FieldDescriptor::TYPE_FIXED32:
    case gp::FieldDescriptor::TYPE_SINT32:
    case gp::FieldDescriptor::TYPE_SFIXED32:
    {
        long value_long = PyLong_AsLong(value);
        if (value_long == -1 && PyErr_Occurred())
        {
            return false;
        }
        if (append)
        {
            _reflection->AddInt32(_message, field, static_cast<int32_t>(value_long));
        }
        else
        {
            _reflection->SetInt32(_message, field, static_cast<int32_t>(value_long));
        }
        break;
    }
    case gp::FieldDescriptor::TYPE_INT64:
    case gp::FieldDescriptor::TYPE_FIXED64:
    case gp::FieldDescriptor::TYPE_SINT64:
    case gp::FieldDescriptor::TYPE_SFIXED64:
    {
        long long value_ll = PyLong_AsLongLong(value);
        if (value_ll == -1 && PyErr_Occurred())
        {
            return false;
        }
        if (append)
        {
            _reflection->AddInt64(_message, field, static_cast<int64_t>(value_ll));
        }
        else
        {
            _reflection->SetInt64(_message, field, static_cast<int64_t>(value_ll));
        }
        break;
    }
    case gp::FieldDescriptor::TYPE_UINT32:
    {
        unsigned long value_ul = PyLong_AsUnsignedLong(value);
        if (value_ul == static_cast<unsigned long>(-1) && PyErr_Occurred())
        {
            return false;
        }
        if (append)
        {
            _reflection->AddUInt32(_message, field, static_cast<uint32_t>(value_ul));
        }
        else
        {
            _reflection->SetUInt32(_message, field, static_cast<uint32_t>(value_ul));
        }
        break;
    }
    case gp::FieldDescriptor::TYPE_UINT64:
    {
        unsigned long long value_ull = PyLong_AsUnsignedLongLong(value);
        if (value_ull == static_cast<unsigned long long>(-1) && PyErr_Occurred())
        {
            return false;
        }
        if (append)
        {
            _reflection->AddUInt64(_message, field, static_cast<uint64_t>(value_ull));
        }
        else
        {
            _reflection->SetUInt64(_message, field, static_cast<uint64_t>(value_ull));
        }
        break;
    }
    case gp::FieldDescriptor::TYPE_BOOL:
    {
        int value_int = PyObject_IsTrue(value);
        if (value_int < 0)
        {
            return false;
        }
        const bool value_bool = value_int != 0;
        append ? _reflection->AddBool(_message, field, value_bool) : _reflection->SetBool(_message, field, value_bool);
        break;
    }
    case gp::FieldDescriptor::TYPE_BYTES:
    {
        Py_buffer buffer;
        if (PyObject_GetBuffer(value, &buffer, PyBUF_SIMPLE) < 0)
        {
            return false;
        }
        std::string const str(reinterpret_cast<char*>(buffer.buf), buffer.len);
        PyBuffer_Release(&buffer);
        append ? _reflection->AddString(_message, field, str) : _reflection->SetString(_message, field, str);
        break;
    }
    case gp::FieldDescriptor::TYPE_STRING:
    {
        const char* str = PyUnicode_AsUTF8(value);
        if (! str)
        {
            return false;
        }
        append ? _reflection->AddString(_message, field, str) : _reflection->SetString(_message, field, str);
        break;
    }
    case gp::FieldDescriptor::TYPE_ENUM:
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
            append ? _reflection->AddEnum(_message, field, enum_value) : _reflection->SetEnum(_message, field, enum_value);
        }
        else
        {
            long value_long = PyLong_AsLong(value);
            if (value_long == -1 && PyErr_Occurred())
            {
                return false;
            }
            if (append)
            {
                _reflection->AddEnumValue(_message, field, static_cast<int>(value_long));
            }
            else
            {
                _reflection->SetEnumValue(_message, field, static_cast<int>(value_long));
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

void PythonMessage::__setattr__(const std::string& field_name, PyObject* value)
{
    auto field = findFieldByNameHack(field_name);
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
            bool ok = setFieldValue(field, item, true);
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

    setFieldValue(field, value, false);
}

PythonMessage* PythonMessage::addRepeatedMessage(const std::string& field_name)
{
    auto field = findFieldByNameHack(field_name);
    if (! field)
    {
        PyErr_SetString(PyExc_AttributeError, field_name.c_str());
        return nullptr;
    }

    gp::Message* message = _reflection->AddMessage(_message, field);
    return new PythonMessage(message);
}

int PythonMessage::repeatedMessageCount(const std::string& field_name) const
{
    auto field = findFieldByNameHack(field_name);
    if (! field)
    {
        PyErr_SetString(PyExc_AttributeError, field_name.c_str());
        return -1;
    }

    return _reflection->FieldSize(*_message, field);
}

PythonMessage* PythonMessage::getMessage(const std::string& field_name)
{
    auto field = findFieldByNameHack(field_name);
    if (! field)
    {
        PyErr_SetString(PyExc_AttributeError, field_name.c_str());
        return nullptr;
    }
    return new PythonMessage(_reflection->MutableMessage(_message, field));
}

PythonMessage* PythonMessage::getRepeatedMessage(const std::string& field_name, int index)
{
    auto field = findFieldByNameHack(field_name);
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

int PythonMessage::getEnumValue(const std::string& enum_value) const
{
    auto field = _descriptor->FindEnumValueByName(enum_value);
    if (! field)
    {
        return -1;
    }

    return field->number();
}

} // namespace Arcus
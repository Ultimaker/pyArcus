#include <Arcus/Socket.h>
#include <Arcus/Error.h>
#include <Arcus/SocketListener.h>
#include <Arcus/Types.h>

#include <google/protobuf/message.h>
#include <pybind11/pybind11.h>

#include "pyArcus/PythonMessage.h"

namespace py = pybind11;

PYBIND11_MODULE(pyArcus, module) {
    module.doc() = R"pbdoc(exit
        pyArcus
        -----------------------
        .. currentmodule:: Python bindings for Arcus
        .. autosummary::
           :toctree: _generate
           add
           subtract
    )pbdoc";

    py::class_<Arcus::Socket>(module, "Socket")
        .def(py::init<>())
        .def("getState", &Arcus::Socket::getState)
        .def("getLastError", &Arcus::Socket::getLastError)
        .def("clearError", &Arcus::Socket::clearError)
        .def("addListener", &Arcus::Socket::addListener)
        .def("removeListener", &Arcus::Socket::removeListener)
        .def("connect", &Arcus::Socket::connect)
        .def("listen", &Arcus::Socket::listen)
        .def("close", &Arcus::Socket::close)
        .def("reset", &Arcus::Socket::reset)
        .def("sendMessage", &Arcus::Socket::sendMessage)
        .def("takeNextMessage", &Arcus::Socket::takeNextMessage)
        .def("createMessage", &Arcus::Socket::createMessage)
        .def("registerAllMessageTypes", &Arcus::Socket::registerAllMessageTypes)
        .def("dumpMessageTypes", &Arcus::Socket::dumpMessageTypes);

    py::class_<Arcus::PythonMessage>(module, "PythonMessage")
        .def("getTypeName", &Arcus::PythonMessage::getTypeName)
        .def("addRepeatedMessage", &Arcus::PythonMessage::addRepeatedMessage)
        .def("repeatedMessageCount", &Arcus::PythonMessage::repeatedMessageCount)
        .def("getRepeatedMessage", &Arcus::PythonMessage::getRepeatedMessage)
        .def("getMessage", &Arcus::PythonMessage::getMessage)
        .def("getEnumValue", &Arcus::PythonMessage::getEnumValue)
        .def("__hasattr__", &Arcus::PythonMessage::__hasattr__)
        .def("__getattr__", &Arcus::PythonMessage::__getattr__)
        .def("__setattr__", &Arcus::PythonMessage::__setattr__);

    py::class_<Arcus::SocketListener>(module, "SocketListener")
        .def(py::init<>())
        .def("getSocket", &Arcus::SocketListener::getSocket)
        .def("stateChanged", &Arcus::SocketListener::stateChanged)
        .def("messageReceived", &Arcus::SocketListener::messageReceived)
        .def("error", &Arcus::SocketListener::error);

    py::enum_<Arcus::SocketState>(module, "SocketState")
        .value("Initial", Arcus::SocketState::Initial)
        .value("Connecting", Arcus::SocketState::Connecting)
        .value("Connected", Arcus::SocketState::Connected)
        .value("Opening", Arcus::SocketState::Opening)
        .value("Listening", Arcus::SocketState::Listening)
        .value("Closing", Arcus::SocketState::Closing)
        .value("Closed", Arcus::SocketState::Closed)
        .value("Error", Arcus::SocketState::Error)
        ;

    py::enum_<Arcus::ErrorCode>(module, "ErrorCode")
        .value("UnknownError", Arcus::ErrorCode::UnknownError)
        .value("CreationError", Arcus::ErrorCode::CreationError)
        .value("ConnectFailedError", Arcus::ErrorCode::ConnectFailedError)
        .value("BindFailedError", Arcus::ErrorCode::BindFailedError)
        .value("AcceptFailedError", Arcus::ErrorCode::AcceptFailedError)
        .value("SendFailedError", Arcus::ErrorCode::SendFailedError)
        .value("ReceiveFailedError", Arcus::ErrorCode::ReceiveFailedError)
        .value("UnknownMessageTypeError", Arcus::ErrorCode::UnknownMessageTypeError)
        .value("ParseFailedError", Arcus::ErrorCode::ParseFailedError)
        .value("ConnectionResetError", Arcus::ErrorCode::ConnectionResetError)
        .value("MessageRegistrationFailedError", Arcus::ErrorCode::MessageRegistrationFailedError)
        .value("InvalidStateError", Arcus::ErrorCode::InvalidStateError)
        .value("InvalidMessageError", Arcus::ErrorCode::InvalidMessageError)
        .value("Debug", Arcus::ErrorCode::Debug);

    py::class_<Arcus::Error>(module, "Error")
        .def(py::init<>())
        .def(py::init<Arcus::ErrorCode, const std::string&>())
        .def("getErrorCode", &Arcus::Error::getErrorCode)
        .def("getErrorMessage", &Arcus::Error::getErrorMessage)
        .def("isFatalError", &Arcus::Error::isFatalError)
        .def("isValid", &Arcus::Error::isValid)
        .def("setFatalError", &Arcus::Error::setFatalError)
        .def("__repr__", &Arcus::Error::toString);
}
cmake_policy(SET CMP0091 NEW)
project(pyarcus)
cmake_minimum_required(VERSION 3.20)

find_package(protobuf REQUIRED)
find_package(cpython REQUIRED)
find_package(arcus REQUIRED)

find_package(pybind11 CONFIG REQUIRED)
pybind11_add_module(pyArcus SHARED src/main.cpp src/PythonMessage.cpp)
use_threads(pyArcus)

target_include_directories(pyArcus
        PRIVATE
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
        )
target_link_libraries(pyArcus PUBLIC arcus::arcus pybind11::pybind11 protobuf::libprotobuf cpython::cpython)

if(NOT MSVC AND NOT ${CMAKE_BUILD_TYPE} MATCHES Debug|RelWithDebInfo)
    pybind11_strip(pyArcus)
endif()


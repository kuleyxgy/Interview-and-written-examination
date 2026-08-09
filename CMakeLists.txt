cmake_minimum_required(VERSION 3.20)

project(sensor_framework LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(CTest)
include(cmake/CompilerWarnings.cmake)

file(GLOB TOOL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tool/*.c")

file(GLOB HOST_SIM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ports/host_sim/*.c")

file(GLOB PROTO_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/*.c")

file(GLOB_RECURSE FUNC_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/func/*.c")

set(SENSOR_FRAMEWORK_SOURCES
    ${TOOL_SOURCES}
    ${HOST_SIM_SOURCES}
    ${PROTO_SOURCES}
    ${FUNC_SOURCES})

add_library(sensor_framework STATIC ${SENSOR_FRAMEWORK_SOURCES})

target_include_directories(sensor_framework PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tool"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/iface"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/func"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/func/filter"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/func/app"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ports/host_sim")

sensor_framework_enable_warnings(sensor_framework)

add_executable(test_tool
    tests/tool/test_tool.c
    ${TOOL_SOURCES})

target_include_directories(test_tool PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tool"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests")

sensor_framework_enable_warnings(test_tool)

add_executable(test_iface
    tests/iface/test_iface.c
    ${HOST_SIM_SOURCES}
    ${TOOL_SOURCES})

target_include_directories(test_iface PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tool"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/iface"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ports/host_sim"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests")

sensor_framework_enable_warnings(test_iface)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/host_demo/main.c")
    add_executable(host_demo examples/host_demo/main.c)
    target_link_libraries(host_demo PRIVATE sensor_framework)
    sensor_framework_enable_warnings(host_demo)
endif()

if(BUILD_TESTING)
    add_test(NAME tool COMMAND test_tool)
    add_test(NAME iface COMMAND test_iface)
endif()

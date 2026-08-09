cmake_minimum_required(VERSION 3.20)

project(sensor_framework LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(CTest)
include(cmake/CompilerWarnings.cmake)

file(GLOB TOOL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tool/*.c")

add_executable(test_tool
    tests/tool/test_tool.c
    ${TOOL_SOURCES})

target_include_directories(test_tool PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tool"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests")

sensor_framework_enable_warnings(test_tool)

if(BUILD_TESTING)
    add_test(NAME tool COMMAND test_tool)
endif()

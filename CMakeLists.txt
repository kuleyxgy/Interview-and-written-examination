cmake_minimum_required(VERSION 3.20)

project(sensor_framework LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(CTest)
include(cmake/CompilerWarnings.cmake)

option(SENSOR_BUILD_EXAMPLES "Build the host demonstration" ON)
option(SENSOR_BUILD_PORT_TEMPLATE "Build the non-functional port template by default" OFF)
set(SENSOR_PORT "host" CACHE STRING "Port linked by sensor_framework (host or none)")
set_property(CACHE SENSOR_PORT PROPERTY STRINGS host none)
if(NOT SENSOR_PORT STREQUAL "host" AND NOT SENSOR_PORT STREQUAL "none")
    message(FATAL_ERROR "SENSOR_PORT must be host or none")
endif()

file(GLOB TOOL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tool/*.c")
file(GLOB HOST_SIM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ports/host_sim/*.c")
file(GLOB PORT_TEMPLATE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ports/template/*.c")
file(GLOB PROTO_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/*.c")
file(GLOB_RECURSE FUNC_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/func/*.c")

# Each architectural layer owns a separate target. The dependency direction is
# tool <- iface/ports <- proto <- func. The composition target only selects a
# concrete port; MCU projects can use SENSOR_PORT=none and link their own port.
add_library(sensor_tool STATIC ${TOOL_SOURCES})
target_include_directories(sensor_tool PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tool")
sensor_framework_enable_warnings(sensor_tool)

add_library(sensor_iface INTERFACE)
target_include_directories(sensor_iface INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/iface")
target_link_libraries(sensor_iface INTERFACE sensor_tool)

if(SENSOR_PORT STREQUAL "host" OR BUILD_TESTING)
    add_library(sensor_port_host STATIC ${HOST_SIM_SOURCES})
else()
    add_library(sensor_port_host STATIC EXCLUDE_FROM_ALL ${HOST_SIM_SOURCES})
endif()
target_include_directories(sensor_port_host PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ports/host_sim")
target_link_libraries(sensor_port_host PUBLIC sensor_iface sensor_tool)
sensor_framework_enable_warnings(sensor_port_host)

if(SENSOR_BUILD_PORT_TEMPLATE)
    add_library(sensor_port_template STATIC ${PORT_TEMPLATE_SOURCES})
else()
    add_library(sensor_port_template STATIC EXCLUDE_FROM_ALL
        ${PORT_TEMPLATE_SOURCES})
endif()
target_link_libraries(sensor_port_template PUBLIC sensor_iface sensor_tool)
sensor_framework_enable_warnings(sensor_port_template)
add_custom_target(check_port_template DEPENDS sensor_port_template)

add_library(sensor_proto STATIC ${PROTO_SOURCES})
target_include_directories(sensor_proto PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/proto")
target_link_libraries(sensor_proto
    PRIVATE sensor_iface
    PUBLIC sensor_tool)
sensor_framework_enable_warnings(sensor_proto)

add_library(sensor_func STATIC ${FUNC_SOURCES})
target_include_directories(sensor_func PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/func"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/func/filter"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/func/app")
target_link_libraries(sensor_func PUBLIC sensor_proto sensor_tool)
sensor_framework_enable_warnings(sensor_func)

add_library(sensor_framework INTERFACE)
target_link_libraries(sensor_framework INTERFACE sensor_func)
if(SENSOR_PORT STREQUAL "host")
    target_link_libraries(sensor_framework INTERFACE sensor_port_host)
endif()

function(sensor_add_test_target target source)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_link_libraries(${target} PRIVATE ${ARGN})
    sensor_framework_enable_warnings(${target})
endfunction()

if(BUILD_TESTING)
    sensor_add_test_target(test_tool tests/tool/test_tool.c sensor_tool)
    sensor_add_test_target(test_iface tests/iface/test_iface.c
        sensor_port_host)
    sensor_add_test_target(test_proto tests/proto/test_proto.c
        sensor_proto sensor_port_host)
    sensor_add_test_target(test_filters tests/func/test_filters.c sensor_func)
    sensor_add_test_target(test_sensor tests/func/test_sensor.c sensor_func)
    sensor_add_test_target(test_apps tests/app/test_apps.c sensor_func)
    sensor_add_test_target(test_mqtt tests/proto/test_mqtt.c
        sensor_proto sensor_port_host)
    sensor_add_test_target(test_e2e tests/e2e/test_e2e.c
        sensor_func sensor_port_host)
    sensor_add_test_target(test_integration tests/e2e/test_dual_sensor_integration.c
        sensor_func sensor_port_host)

    add_test(NAME tool COMMAND test_tool)
    add_test(NAME iface COMMAND test_iface)
    add_test(NAME proto COMMAND test_proto)
    add_test(NAME filters COMMAND test_filters)
    add_test(NAME sensor COMMAND test_sensor)
    add_test(NAME apps COMMAND test_apps)
    add_test(NAME mqtt COMMAND test_mqtt)
    add_test(NAME e2e COMMAND test_e2e)
    add_test(NAME integration COMMAND test_integration)
endif()

if(SENSOR_BUILD_EXAMPLES AND SENSOR_PORT STREQUAL "host")
    add_executable(host_demo examples/host_demo/main.c)
    target_link_libraries(host_demo PRIVATE sensor_framework)
    sensor_framework_enable_warnings(host_demo)
    if(BUILD_TESTING)
        add_test(NAME host_demo_e2e COMMAND host_demo)
        set_tests_properties(host_demo_e2e PROPERTIES
            PASS_REGULAR_EXPRESSION
            "\\[SUMMARY\\] GUI_records=[1-9][0-9]* MQTT_pending=0 MQTT_dropped=[1-9]")
    endif()
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_target(check_layers
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_layers.py"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
    if(BUILD_TESTING)
        add_test(NAME layers
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_layers.py")
        add_test(NAME layers_negative
            COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tool/test_check_layers.py"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_layers.py"
                    "${CMAKE_CURRENT_BINARY_DIR}/layer-fixtures")
    endif()
endif()

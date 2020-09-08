# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EXPECTED_IDF_VERSION v4.0.1)

set(EXTRA_COMPONENT_DIRS
        components/ruuvi.comm_tester.c/components)

execute_process (
        COMMAND bash -c "cd $ENV{IDF_PATH} && git describe --always --tags --dirty --match ${EXPECTED_IDF_VERSION}*"
        OUTPUT_VARIABLE IDF_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

if("${IDF_VERSION}" STREQUAL "${EXPECTED_IDF_VERSION}")
    message(esp-idf version: ${IDF_VERSION})
elseif("${IDF_VERSION}" STREQUAL "${EXPECTED_IDF_VERSION}-dirty")
    message(WARNING "Expected esp-idf version is ${EXPECTED_IDF_VERSION}, but actual version is ${IDF_VERSION}")
else()
    message(FATAL_ERROR "Expected esp-idf version is ${EXPECTED_IDF_VERSION}, but actual version is ${IDF_VERSION}")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ruuvi_gateway_esp)

target_compile_definitions(__idf_main PUBLIC
        RUUVI_LEDS_DISABLE=1
        RUUVI_ESP
)

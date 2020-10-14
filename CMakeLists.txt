# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EXPECTED_IDF_VERSION v4.0.1)
set(RUUVI_NRF52_FW_URL https://github.com/ruuvi/ruuvi.gateway_nrf.c/releases/download/v0.5.0/ruuvigw_nrf_armgcc_ruuvigw_release_v0.5.0_full.hex)

set(EXTRA_COMPONENT_DIRS
        components/ruuvi.comm_tester.c/components)

include(ExternalProject)

ExternalProject_Add(mkfatfs
        PREFIX mkfatfs
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/mkfatfs
        BINARY_DIR ${CMAKE_BINARY_DIR}/mkfatfs
        BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/mkfatfs/mkfatfs
        INSTALL_COMMAND ""
        BUILD_ALWAYS 1
)

string(REGEX MATCH ".*/(v[0-9]+.[0-9]+.[0-9]+)/[^/]*" _ ${RUUVI_NRF52_FW_URL})
set(RUUVI_NRF52_FW_VERSION ${CMAKE_MATCH_1})
if("${RUUVI_NRF52_FW_VERSION}" STREQUAL "")
    message(FATAL_ERROR "Bad nRF52 firmware version: ${RUUVI_NRF52_FW_URL}")
endif()

set(GW_NRF_PARTITION fatfs_nrf52)
file(STRINGS "partitions.csv" PARTITIONS_CSV)
string(REGEX MATCH "${GW_NRF_PARTITION}, *data, *fat, *(0x[0-9a-fA-F]+), *([x0-9a-fA-F]+)," PARTITION_FATFS_NRF52 ${PARTITIONS_CSV})
if("${PARTITION_FATFS_NRF52}" STREQUAL "")
    message(FATAL_ERROR "Can't find 'fatfs_nrf52' partition in partitions.csv")
endif()
set(FATFS_NRF52_ADDR ${CMAKE_MATCH_1})
set(FATFS_NRF52_SIZE ${CMAKE_MATCH_2})


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
        GW_NRF_PARTITION="${GW_NRF_PARTITION}"
        BOARD_RUUVIGW_ESP
)

# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffile-prefix-map=${CMAKE_SOURCE_DIR}=.")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffile-prefix-map=$ENV{IDF_PATH}=IDF")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffile-prefix-map=${CMAKE_SOURCE_DIR}=.")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffile-prefix-map=$ENV{IDF_PATH}=IDF")

set(EXPECTED_IDF_VERSION v4.2.2)
set(RUUVI_NRF52_FW_URL https://github.com/ruuvi/ruuvi.gateway_nrf.c/releases/download/v1.0.0/ruuvigw_nrf_armgcc_ruuvigw_release_v1.0.0_full.hex)

set(EXTRA_COMPONENT_DIRS
        ${CMAKE_SOURCE_DIR}/components/ruuvi.comm_tester.c/components
)

set(EXCLUDE_COMPONENTS "nghttp2" "openssl" "libsodium" "coap" "protocomm" "freemodbus")

include(ExternalProject)

ExternalProject_Add(mkfatfs
        PREFIX mkfatfs
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/mkfatfs
        BINARY_DIR ${CMAKE_BINARY_DIR}/mkfatfs
        BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/mkfatfs/mkfatfs
        INSTALL_COMMAND ""
        BUILD_ALWAYS 1
)

string(REGEX MATCH ".*/v?([0-9]+.[0-9]+.[0-9]+)/[^/]*" _ ${RUUVI_NRF52_FW_URL})
set(RUUVI_NRF52_FW_VERSION v${CMAKE_MATCH_1})
if("${RUUVI_NRF52_FW_VERSION}" STREQUAL "v")
    message(FATAL_ERROR "Bad nRF52 firmware version: ${RUUVI_NRF52_FW_URL}")
endif()

set(GW_GWUI_PARTITION fatfs_gwui)
file(STRINGS "partitions.csv" PARTITIONS_CSV)
string(REGEX MATCH "${GW_GWUI_PARTITION}, *data, *fat, *(0x[0-9a-fA-F]+), *([x0-9a-fA-F]+)," PARTITION_FATFS_GWUI ${PARTITIONS_CSV})
if("${PARTITION_FATFS_GWUI}" STREQUAL "")
    message(FATAL_ERROR "Can't find 'fatfs_gwui' partition in partitions.csv")
endif()
set(FATFS_GWUI_ADDR ${CMAKE_MATCH_1})
set(FATFS_GWUI_SIZE ${CMAKE_MATCH_2})

set(GW_NRF_PARTITION fatfs_nrf52)
file(STRINGS "partitions.csv" PARTITIONS_CSV)
string(REGEX MATCH "${GW_NRF_PARTITION}, *data, *fat, *(0x[0-9a-fA-F]+), *([x0-9a-fA-F]+)," PARTITION_FATFS_NRF52 ${PARTITIONS_CSV})
if("${PARTITION_FATFS_NRF52}" STREQUAL "")
    message(FATAL_ERROR "Can't find 'fatfs_nrf52' partition in partitions.csv")
endif()
set(FATFS_NRF52_ADDR ${CMAKE_MATCH_1})
set(FATFS_NRF52_SIZE ${CMAKE_MATCH_2})

set(GW_CFG_PARTITION gw_cfg_def)
file(STRINGS "partitions.csv" PARTITIONS_CSV)
string(REGEX MATCH "${GW_CFG_PARTITION}, *data, *nvs, *(0x[0-9a-fA-F]+), *([x0-9a-fA-F]+)," PARTITION_GW_CFG_DEF ${PARTITIONS_CSV})
if("${PARTITION_GW_CFG_DEF}" STREQUAL "")
    message(FATAL_ERROR "Can't find 'gw_cfg_def' partition in partitions.csv")
endif()
set(GW_CFG_DEF_ADDR ${CMAKE_MATCH_1})
set(GW_CFG_DEF_SIZE ${CMAKE_MATCH_2})

execute_process (
        COMMAND bash -c "git describe --always --tags --dirty"
        OUTPUT_VARIABLE PROJECT_VER
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
message("Project version: ${PROJECT_VER}")

execute_process (
        COMMAND bash -c "cd $ENV{IDF_PATH} && git describe --always --tags --dirty --match ${EXPECTED_IDF_VERSION}*"
        OUTPUT_VARIABLE IDF_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

if("${IDF_VERSION}" STREQUAL "${EXPECTED_IDF_VERSION}")
    message("esp-idf version: ${IDF_VERSION}")
elseif("${IDF_VERSION}" STREQUAL "${EXPECTED_IDF_VERSION}-dirty")
    message(WARNING "Expected esp-idf version is ${EXPECTED_IDF_VERSION}, but actual version is ${IDF_VERSION}")
else()
    message(FATAL_ERROR "Expected esp-idf version is ${EXPECTED_IDF_VERSION}, but actual version is ${IDF_VERSION}")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ruuvi_gateway_esp)

add_custom_target(check_json_schemas
        COMMAND ${CMAKE_COMMAND} -E echo "Running Json Schemas checks..."
        COMMAND ${CMAKE_SOURCE_DIR}/schemas/check_schemas.sh
        COMMENT "This target executes checks for Json Schemas."
)

target_compile_definitions(__idf_main PUBLIC
        RUUVI_ESP
        GW_GWUI_PARTITION="${GW_GWUI_PARTITION}"
        GW_NRF_PARTITION="${GW_NRF_PARTITION}"
        GW_CFG_PARTITION="${GW_CFG_PARTITION}"
        BOARD_RUUVIGW_ESP
)

target_compile_definitions(__idf_esp_netif PUBLIC "-DLWIP_DHCP_GET_NTP_SRV=1")
target_compile_definitions(__idf_mqtt PUBLIC "-DCONFIG_MQTT_OUTBOX_MAX_SIZE=4096" "-DCONFIG_MQTT_BUFFER_SIZE=1024" "-DCONFIG_MQTT_TASK_PRIORITY=7")

target_include_directories(__idf_lwip BEFORE PUBLIC ./include)

target_include_directories(__idf_esp_common BEFORE PUBLIC ./components/esp-tls)

target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "partition_table/partition-table.bin" BINARY)

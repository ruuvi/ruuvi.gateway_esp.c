# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(FATFS_GWUI_IMG ${CMAKE_BINARY_DIR}/fatfs_gwui.bin)
set(GWUI_UNSIGNED "${FATFS_GWUI_IMG}")
set(GWUI_SIGNED   "${GWUI_UNSIGNED}.signed")
set(GWUI_SIGNATURE "${GWUI_UNSIGNED}.signature")

set(FATFS_NRF52_IMG ${CMAKE_BINARY_DIR}/fatfs_nrf52.bin)
set(NRF52_UNSIGNED "${FATFS_NRF52_IMG}")
set(NRF52_SIGNED   "${NRF52_UNSIGNED}.signed")
set(NRF52_SIGNATURE "${NRF52_UNSIGNED}.signature")
set(NRF52_FATFS_WL_AREA ${CMAKE_BINARY_DIR}/fatfs_nrf52_wl_area.bin)

add_compile_definitions(RI_ADV_EXTENDED_ENABLED=1 RI_COMM_BLE_PAYLOAD_MAX_LENGTH=48)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffile-prefix-map=${CMAKE_SOURCE_DIR}=.")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffile-prefix-map=$ENV{IDF_PATH}=IDF")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffile-prefix-map=${CMAKE_SOURCE_DIR}=.")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffile-prefix-map=$ENV{IDF_PATH}=IDF")

set(EXPECTED_IDF_VERSION v4.2.2)

# ==============================================================================
# nRF52 Firmware Configuration
# ==============================================================================

set(RUUVI_NRF52_FW_VERSION "v2.0.0")

# Define which nRF52 firmware to use:
set(RUUVI_NRF52_FW_CONFIG "Prod-RuuviGateway")
#set(RUUVI_NRF52_FW_CONFIG "Dev-RuuviGateway")
#set(RUUVI_NRF52_FW_CONFIG "Dev-nRF52840-DK")
#set(RUUVI_NRF52_FW_CONFIG "Dev-nRF52-DK")

# Configure firmware URL based on selected configuration
if(RUUVI_NRF52_FW_CONFIG STREQUAL "Prod-RuuviGateway")
    set(RUUVI_NRF52_FW_TARGET "ruuvigw_nrf")
    set(RUUVI_NRF52_FW_URL "https://github.com/ruuvi/ruuvi.gateway_nrf.c/releases/download/${RUUVI_NRF52_FW_VERSION}/${RUUVI_NRF52_FW_TARGET}_armgcc_ruuvigw_release_${RUUVI_NRF52_FW_VERSION}_full.hex")
    message(STATUS "nRF52 FW URL: ${RUUVI_NRF52_FW_URL}")
else()
    set(RUUVI_NRF52_FW_BASE_DIR "${CMAKE_SOURCE_DIR}/../ruuvi.gateway_nrf.c/src")
    message(STATUS "nRF52 FW base dir: ${RUUVI_NRF52_FW_BASE_DIR}")

    if(RUUVI_NRF52_FW_CONFIG STREQUAL "Dev-RuuviGateway")
        set(RUUVI_NRF52_FW_TARGET "ruuvigw_nrf")
        message(STATUS "nRF52 FW: Development - Ruuvi Gateway (local)")
    elseif(RUUVI_NRF52_FW_CONFIG STREQUAL "Dev-nRF52840-DK")
        set(RUUVI_NRF52_FW_TARGET "pca10059")
        message(STATUS "nRF52 FW: Development - nRF52840 DK (local)")
    elseif(RUUVI_NRF52_FW_CONFIG STREQUAL "Dev-nRF52-DK")
        set(RUUVI_NRF52_FW_TARGET "pca10040")
        message(STATUS "nRF52 FW: Development - nRF52 DK (local)")
    else()
        message(FATAL_ERROR "Invalid RUUVI_NRF52_FW_CONFIG value: ${RUUVI_NRF52_FW_CONFIG}")
    endif()
endif()

# ==============================================================================

set(EXTRA_COMPONENT_DIRS
        ${CMAKE_SOURCE_DIR}/components/ruuvi.comm_tester.c/components
)

set(EXCLUDE_COMPONENTS "nghttp2" "openssl" "libsodium" "coap" "protocomm" "freemodbus")

include(ExternalProject)

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
target_compile_definitions(__idf_esp-tls PUBLIC "-DCONFIG_LWIP_IPV4=1" "-DCONFIG_LWIP_IPV6=0")
target_include_directories(__idf_lwip BEFORE PUBLIC ./include)

target_include_directories(__idf_esp_common BEFORE PUBLIC ./components/esp-tls)

target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "partition_table/partition-table.bin" BINARY)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "nrf52_sha256/sha256_stub.bin" BINARY)

# Only add if the signature-producing target exists (i.e., signing enabled)
if(TARGET gen_nrf52_signature)
    # Ensure the file path exists at configure time so target_add_binary_data doesn't error
    if(NOT EXISTS "${NRF52_SIGNATURE}")
        file(WRITE "${NRF52_SIGNATURE}" "")
    endif()
    # Tell CMake this file will be generated/updated during the build
    set_source_files_properties("${NRF52_SIGNATURE}" PROPERTIES GENERATED TRUE)

    # Embed the signature as raw binary data into the ELF
    target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${NRF52_SIGNATURE}" BINARY)
    # Ensure the signature is generated before the ELF is linked
    add_dependencies(${CMAKE_PROJECT_NAME}.elf gen_nrf52_signature)
endif()

# Only add if the signature-producing target exists (i.e., signing enabled)
if(TARGET gen_gwui_signature)
    # Ensure the file path exists at configure time so target_add_binary_data doesn't error
    if(NOT EXISTS "${GWUI_SIGNATURE}")
        file(WRITE "${GWUI_SIGNATURE}" "")
    endif()
    set_source_files_properties("${GWUI_SIGNATURE}" PROPERTIES GENERATED TRUE)

    # Embed the signature as raw binary data into the ELF
    target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${GWUI_SIGNATURE}" BINARY)
    # Ensure the signature is generated before the ELF is linked
    add_dependencies(${CMAKE_PROJECT_NAME}.elf gen_gwui_signature)
endif()

# Ensure the file path exists at configure time so target_add_binary_data doesn't error
if(NOT EXISTS "${NRF52_FATFS_WL_AREA}")
    file(WRITE "${NRF52_FATFS_WL_AREA}" "")
endif()
# Tell CMake this file will be generated/updated during the build
set_source_files_properties("${NRF52_FATFS_WL_AREA}" PROPERTIES GENERATED TRUE)

# Embed the nrf52_fatfs_wl_area as raw binary data into the ELF
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${NRF52_FATFS_WL_AREA}" BINARY)
# Ensure the nrf52_fatfs_wl_area is generated before the ELF is linked
add_dependencies(${CMAKE_PROJECT_NAME}.elf gen_nrf52_fatfs_wl_area)

# Add a post-build command to the project ELF to automatically show memory usage.
idf_build_get_property(idf_path IDF_PATH)
idf_build_get_property(python PYTHON)
add_custom_command(TARGET ${CMAKE_PROJECT_NAME}.elf POST_BUILD
        COMMAND ${python} ${idf_path}/tools/idf_size.py
        --target ${IDF_TARGET}
        ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.map
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Showing memory usage"
        USES_TERMINAL
)

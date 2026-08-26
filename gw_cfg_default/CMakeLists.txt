cmake_minimum_required(VERSION 3.22)

project(gw_cfg_default)

if(NOT DEFINED ENV{IDF_PATH} OR "$ENV{IDF_PATH}" STREQUAL "")
    message(FATAL_ERROR "IDF_PATH is not set. Source ESP-IDF export.sh (e.g. source ~/esp-idf-env.sh) before building gw_cfg_default.")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

add_definitions(
        -DRUUVI_TESTS=1
        -D_GNU_SOURCE
)

set(COMPONENTS ${CMAKE_CURRENT_SOURCE_DIR}/../components)
set(TESTS ${CMAKE_CURRENT_SOURCE_DIR}/../tests)

set(RUUVI_ESP_WRAPPERS ${COMPONENTS}/ruuvi.esp_wrappers.c)
set(RUUVI_ESP_WRAPPERS_INC ${RUUVI_ESP_WRAPPERS}/include)

set(RUUVI_ESP_WRAPPERS_TESTS_COMMON ${RUUVI_ESP_WRAPPERS}/tests/common)
set(RUUVI_ESP_WRAPPERS_TESTS_COMMON_INC ${RUUVI_ESP_WRAPPERS_TESTS_COMMON}/include)

set(RUUVI_JSON_STREAM_GEN ${COMPONENTS}/ruuvi.json_stream_gen.c)
set(RUUVI_JSON_STREAM_GEN_SRC ${RUUVI_JSON_STREAM_GEN}/src)
set(RUUVI_JSON_STREAM_GEN_INC ${RUUVI_JSON_STREAM_GEN}/include)

include_directories(
        ${RUUVI_ESP_WRAPPERS_INC}
        ${RUUVI_ESP_WRAPPERS_TESTS_COMMON_INC}
)

set(RUUVI_GW_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../main)

set(WIFI_MANAGER ${COMPONENTS}/esp32-wifi-manager)
set(WIFI_MANAGER_SRC ${WIFI_MANAGER}/src)
set(WIFI_MANAGER_INC ${WIFI_MANAGER}/src/include)

include_directories(
        "${TESTS}/posix_gcc_simulator/FreeRTOS_Posix"
        "${TESTS}/posix_gcc_simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include"
        "${TESTS}/posix_gcc_simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include/freertos"
        "${TESTS}/posix_gcc_simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix"
)

add_subdirectory(${RUUVI_ESP_WRAPPERS} ruuvi.esp_wrappers.c)
add_subdirectory(${RUUVI_ESP_WRAPPERS_TESTS_COMMON} ruuvi.esp_wrappers_tests)

include_directories(
        "${TESTS}/lwip/lwip/src/include"
        "${TESTS}/lwip/extra"
        "${TESTS}/lwip/lwip/contrib/ports/freertos/include"
        "${TESTS}/lwip/lwip/contrib/ports/unix/port/include"
)

include_directories(
        ${RUUVI_GW_SRC}
        ${TESTS}/include
        ${WIFI_MANAGER_INC}
        ${RUUVI_JSON_STREAM_GEN_INC}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${COMPONENTS}/nvs_flash/include
        ${COMPONENTS}/spi_flash/include
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include
        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/spi_flash/include
        $ENV{IDF_PATH}/components/esp_wifi/include
        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/json/cJSON
)

add_executable(gw_cfg_default
        gw_cfg_default_gen.c
        ${RUUVI_GW_SRC}/cjson_wrap.c
        ${RUUVI_GW_SRC}/cjson_wrap.h
        ${RUUVI_GW_SRC}/gw_cfg.c
        ${RUUVI_GW_SRC}/gw_cfg.h
        ${RUUVI_GW_SRC}/gw_cfg_cmp.c
        ${RUUVI_GW_SRC}/gw_cfg_default.c
        ${RUUVI_GW_SRC}/gw_cfg_default.h
        ${RUUVI_GW_SRC}/gw_cfg_json_generate.c
        ${RUUVI_GW_SRC}/gw_cfg_json_generate.h
        ${RUUVI_GW_SRC}/gw_cfg_json_generate_internal.c
        ${RUUVI_GW_SRC}/gw_cfg_json_generate_internal.h
        ${RUUVI_GW_SRC}/gw_cfg_json_generate_remote.c
        ${RUUVI_GW_SRC}/gw_cfg_json_generate_remote.h
        ${RUUVI_GW_SRC}/gw_cfg_storage.c
        ${RUUVI_GW_SRC}/gw_cfg_storage.h
        ${RUUVI_GW_SRC}/gw_cfg_storage_files.c
        ${RUUVI_ESP_WRAPPERS}/src/str_buf.c
        ${RUUVI_ESP_WRAPPERS_INC}/str_buf.h
        ${RUUVI_ESP_WRAPPERS}/src/os_str.c
        ${RUUVI_ESP_WRAPPERS_INC}/os_str.h
        ${RUUVI_ESP_WRAPPERS}/src/snprintf_with_esp_err_desc.c
        ${RUUVI_ESP_WRAPPERS_INC}/snprintf_with_esp_err_desc.h
        ${RUUVI_ESP_WRAPPERS}/src/os_malloc.c
        ${RUUVI_ESP_WRAPPERS_INC}/os_malloc.h
        ${RUUVI_ESP_WRAPPERS}/src/mac_addr.c
        ${RUUVI_ESP_WRAPPERS_INC}/mac_addr.h
        ${WIFI_MANAGER_SRC}/http_server_auth_type.c
        ${WIFI_MANAGER_INC}/http_server_auth_type.h
        ${WIFI_MANAGER_SRC}/wifi_manager_default_config.c
        ${WIFI_MANAGER_SRC}/wifiman_config.c
        ${WIFI_MANAGER_SRC}/wifiman_md5.c
        ${WIFI_MANAGER_INC}/wifiman_md5.h
        ${WIFI_MANAGER_INC}/wifi_manager.h
        ${WIFI_MANAGER_INC}/wifi_manager_defs.h
        ${TESTS}/lwip/lwip/src/core/ipv4/ip4_addr.c
        ${TESTS}/lwip/lwip/src/core/def.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/md5.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/platform_util.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include/mbedtls/platform_util.h
        $ENV{IDF_PATH}/components/json/cJSON/cJSON.c
        $ENV{IDF_PATH}/components/json/cJSON/cJSON.h
)

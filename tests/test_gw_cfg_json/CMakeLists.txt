cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-gw_cfg_json)
set(ProjectId ruuvi_gateway_esp-test-gw_cfg_json)

add_executable(${ProjectId}
        test_gw_cfg_json.cpp
        sdkconfig.h
        ${RUUVI_GW_SRC}/gw_cfg.c
        ${RUUVI_GW_SRC}/gw_cfg.h
        ${RUUVI_GW_SRC}/gw_cfg_blob.c
        ${RUUVI_GW_SRC}/gw_cfg_blob.h
        ${RUUVI_GW_SRC}/gw_cfg_cmp.c
        ${RUUVI_GW_SRC}/gw_cfg_default.c
        ${RUUVI_GW_SRC}/gw_cfg_default.h
        ${RUUVI_GW_SRC}/gw_cfg_json_parse.c
        ${RUUVI_GW_SRC}/gw_cfg_json_parse.h
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_auto_update.c
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_auto_update.h
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_device_info.c
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_device_info.h
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_eth.c
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_eth.h
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_filter.c
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_filter.h
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_fw_update.c
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_fw_update.h
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_http.c
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_http.h
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_http_stat.c
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_http_stat.h
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_internal.c
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_internal.h
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_lan_auth.c
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_lan_auth.h
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_mqtt.c
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_mqtt.h
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_ntp.c
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_ntp.h
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_remote.c
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_remote.h
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_scan.c
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_scan.h
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_scan_filter.c
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_scan_filter.h
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_wifi.c
        ${RUUVI_GW_SRC}/gw_cfg_json_parse_wifi.h
        ${RUUVI_GW_SRC}/gw_cfg_json_generate.c
        ${RUUVI_GW_SRC}/gw_cfg_json_generate.h
        ${RUUVI_GW_SRC}/gw_cfg_storage_files.c
        ${RUUVI_GW_SRC}/gw_cfg_storage.h
        ${RUUVI_GW_SRC}/gw_cfg_log.c
        ${RUUVI_GW_SRC}/gw_cfg_log.h
        ${RUUVI_GW_SRC}/cjson_wrap.c
        ${RUUVI_GW_SRC}/cjson_wrap.h
        ${RUUVI_GW_SRC}/event_mgr.h
        ${WIFI_MANAGER_SRC}/wifiman_config.c
        ${WIFI_MANAGER_SRC}/wifiman_config.h
        ${WIFI_MANAGER_SRC}/wifiman_md5.c
        ${WIFI_MANAGER_INC}/wifiman_md5.h
        ${WIFI_MANAGER_SRC}/http_server_auth_type.c
        ${WIFI_MANAGER_INC}/http_server_auth_type.h
        ${WIFI_MANAGER_SRC}/wifi_manager_default_config.c
        ${WIFI_MANAGER_INC}/wifi_manager.h
        ${RUUVI_ESP_WRAPPERS}/src/mac_addr.c
        ${RUUVI_ESP_WRAPPERS}/include/mac_addr.h
        ${RUUVI_ESP_WRAPPERS}/src/str_buf.c
        ${RUUVI_ESP_WRAPPERS}/include/str_buf.h
        ${RUUVI_ESP_WRAPPERS}/src/os_str.c
        ${RUUVI_ESP_WRAPPERS}/include/os_str.h
        $ENV{IDF_PATH}/components/esp_common/src/esp_err_to_name.c
        $ENV{IDF_PATH}/components/lwip/lwip/src/core/ipv4/ip4_addr.c
        $ENV{IDF_PATH}/components/lwip/lwip/src/core/def.c
        $ENV{IDF_PATH}/components/json/cJSON/cJSON.c
        $ENV{IDF_PATH}/components/json/cJSON/cJSON.h
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/md5.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/platform_util.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include/mbedtls/platform_util.h
)

set_target_properties(${ProjectId} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 14
)

target_include_directories(${ProjectId} PUBLIC
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        ${RUUVI_GW_SRC}
        ${WIFI_MANAGER_INC}
        include
        $ENV{IDF_PATH}/components/json/cJSON
        $ENV{IDF_PATH}/components/app_update/include
        $ENV{IDF_PATH}/components/spi_flash/include
        $ENV{IDF_PATH}/components/soc/include
        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/bootloader_support/include
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include
        $ENV{IDF_PATH}/components/esp_wifi/include
        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/esp_event/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${RUUVI_JSON_STREAM_GEN_INC}
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_GW_CFG_JSON=1
        CONFIG_DEFAULT_AP_SSID=\"RuuviGateway\"
)

target_compile_options(${ProjectId} PUBLIC
        -g3
        -ggdb
        -fprofile-arcs
        -ftest-coverage
        --coverage
)

# CMake has a target_link_options starting from version 3.13
#target_link_options(${ProjectId} PUBLIC
#        --coverage
#)

target_link_libraries(${ProjectId}
        gtest
        gtest_main
        gcov
        ruuvi_esp_wrappers-common_test_funcs
        --coverage
)

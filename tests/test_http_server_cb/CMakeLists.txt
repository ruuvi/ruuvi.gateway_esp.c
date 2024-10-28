cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-http_server_cb)
set(ProjectId ruuvi_gateway_esp-test-http_server_cb)

add_executable(${ProjectId}
        test_http_server_cb.cpp
        ${RUUVI_GW_SRC}/adv_decode_0x05.c
        ${RUUVI_GW_SRC}/adv_decode_0x06.c
        ${RUUVI_GW_SRC}/adv_decode_0xe0.c
        ${RUUVI_GW_SRC}/adv_decode_0xf0.c
        ${RUUVI_GW_SRC}/adv_decode.h
        ${RUUVI_GW_SRC}/adv_table.h
        ${RUUVI_GW_SRC}/bin2hex.c
        ${RUUVI_GW_SRC}/bin2hex.h
        ${RUUVI_GW_SRC}/cjson_wrap.c
        ${RUUVI_GW_SRC}/cjson_wrap.h
        ${RUUVI_GW_SRC}/gw_cfg.c
        ${RUUVI_GW_SRC}/gw_cfg.h
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
        ${RUUVI_GW_SRC}/gw_cfg_log.c
        ${RUUVI_GW_SRC}/gw_cfg_log.h
        ${RUUVI_GW_SRC}/gw_cfg_ruuvi_json.c
        ${RUUVI_GW_SRC}/gw_cfg_ruuvi_json.h
        ${RUUVI_GW_SRC}/gw_cfg_storage_files.c
        ${RUUVI_GW_SRC}/gw_cfg_storage.h
        ${RUUVI_GW_SRC}/gw_mac.c
        ${RUUVI_GW_SRC}/gw_mac.h
        ${RUUVI_GW_SRC}/http.h
        ${RUUVI_GW_SRC}/http_download.c
        ${RUUVI_GW_SRC}/http_download.h
        ${RUUVI_GW_SRC}/http_server_cb.c
        ${RUUVI_GW_SRC}/http_server_cb.h
        ${RUUVI_GW_SRC}/http_server_cb_on_get.c
        ${RUUVI_GW_SRC}/http_server_cb_on_post.c
        ${RUUVI_GW_SRC}/http_json.c
        ${RUUVI_GW_SRC}/http_json.h
        ${RUUVI_GW_SRC}/json_ruuvi.c
        ${RUUVI_GW_SRC}/json_ruuvi.h
        ${RUUVI_GW_SRC}/metrics.h
        ${RUUVI_GW_SRC}/time_str.c
        ${RUUVI_GW_SRC}/time_str.h
        ${RUUVI_GW_SRC}/event_mgr.h
        ${RUUVI_GW_SRC}/url_encode.c
        ${RUUVI_GW_SRC}/url_encode.h
        ${RUUVI_GW_SRC}/validate_url.c
        ${RUUVI_GW_SRC}/validate_url.h
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_5.c
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_5.h
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_6.c
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_6.h
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_f0.c
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_f0.h
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_e0.c
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_e0.h
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoints.c
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoints.h
        ${RUUVI_JSON_STREAM_GEN_SRC}/json_stream_gen.c
        ${RUUVI_JSON_STREAM_GEN_INC}/json_stream_gen.h
        ${RUUVI_ESP_WRAPPERS_INC}/log.h
        ${RUUVI_ESP_WRAPPERS_INC}/mac_addr.h
        ${RUUVI_ESP_WRAPPERS_INC}/os_mkgmtime.h
        ${RUUVI_ESP_WRAPPERS_INC}/os_str.h
        ${RUUVI_ESP_WRAPPERS_INC}/snprintf_with_esp_err_desc.h
        ${RUUVI_ESP_WRAPPERS_INC}/str_buf.h
        ${RUUVI_ESP_WRAPPERS_INC}/wrap_esp_err_to_name_r.h
        ${RUUVI_ESP_WRAPPERS}/src/mac_addr.c
        ${RUUVI_ESP_WRAPPERS}/src/os_mkgmtime.c
        ${RUUVI_ESP_WRAPPERS}/src/os_str.c
        ${RUUVI_ESP_WRAPPERS}/src/snprintf_with_esp_err_desc.c
        ${RUUVI_ESP_WRAPPERS}/src/str_buf.c
        ${WIFI_MANAGER_SRC}/http_server_auth.c
        ${WIFI_MANAGER_SRC}/http_server_auth.h
        ${WIFI_MANAGER_SRC}/http_server_auth_type.c
        ${WIFI_MANAGER_INC}/http_server_auth_type.h
        ${WIFI_MANAGER_SRC}/http_server_resp.c
        ${WIFI_MANAGER_INC}/http_server_resp.h
        ${WIFI_MANAGER_SRC}/wifiman_config.c
        ${WIFI_MANAGER_SRC}/wifiman_config.h
        ${WIFI_MANAGER_INC}/wifi_manager.h
        ${WIFI_MANAGER_INC}/wifi_manager_defs.h
        ${WIFI_MANAGER_SRC}/wifiman_sha256.c
        ${WIFI_MANAGER_SRC}/wifiman_sha256.h
        ${WIFI_MANAGER_SRC}/wifiman_md5.c
        ${WIFI_MANAGER_SRC}/include/wifiman_md5.h
        ${WIFI_MANAGER_SRC}/wifi_manager_default_config.c
        ${WIFI_MANAGER_INC}/wifi_manager.h
        $ENV{IDF_PATH}/components/esp_common/src/esp_err_to_name.c
        $ENV{IDF_PATH}/components/lwip/lwip/src/core/ipv4/ip4_addr.c
        $ENV{IDF_PATH}/components/lwip/lwip/src/core/def.c
        $ENV{IDF_PATH}/components/json/cJSON/cJSON.c
        $ENV{IDF_PATH}/components/json/cJSON/cJSON.h
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/md5.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include/mbedtls/md5.h
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/sha256.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include/mbedtls/sha256.h
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/platform_util.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include/mbedtls/platform_util.h
        $ENV{IDF_PATH}/components/nghttp/nghttp2/third-party/http-parser/http_parser.c
        $ENV{IDF_PATH}/components/nghttp/nghttp2/third-party/http-parser/http_parser.h
)

set_target_properties(${ProjectId} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 14
)

target_include_directories(${ProjectId} PUBLIC
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        ${RUUVI_GW_SRC}
        include
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src
        ${COMPONENTS}/esp_http_client/include
        $ENV{IDF_PATH}/components/json/cJSON
        $ENV{IDF_PATH}/components/esp_wifi/include
        $ENV{IDF_PATH}/components/tcpip_adapter/include
        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/esp_event/include
        $ENV{IDF_PATH}/components/lwip/include/apps
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include
        $ENV{IDF_PATH}/components/app_update/include
        $ENV{IDF_PATH}/components/spi_flash/include
        $ENV{IDF_PATH}/components/soc/include
        $ENV{IDF_PATH}/components/nghttp/port/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ../../components/esp32-wifi-manager/src/include
        ../../components/esp32-wifi-manager/src
        ../lwip/extra
        ../lwip/lwip/src/include
        ../lwip/lwip/contrib/ports/unix/lib
        ../lwip/lwip/contrib/ports/unix/port/include
        ${RUUVI_JSON_STREAM_GEN_INC}
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_HTTP_SERVER_CB=1
        CONFIG_DEFAULT_AP_SSID=\"RuuviGateway\"
        GW_GWUI_PARTITION="fatfs_gwui"
        GW_NRF_PARTITION="fatfs_nrf52"
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

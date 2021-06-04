cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-http_server_cb)
set(ProjectId ruuvi_gateway_esp-test-http_server_cb)

add_executable(${ProjectId}
        test_http_server_cb.cpp
        ${RUUVI_GW_SRC}/adv_table.h
        ${RUUVI_GW_SRC}/gw_cfg.c
        ${RUUVI_GW_SRC}/gw_cfg.h
        ${RUUVI_GW_SRC}/http_server_cb.c
        ${RUUVI_GW_SRC}/http_server_cb.h
        ${RUUVI_GW_SRC}/http_json.c
        ${RUUVI_GW_SRC}/http_json.h
        ${RUUVI_GW_SRC}/bin2hex.c
        ${RUUVI_GW_SRC}/bin2hex.h
        ${RUUVI_GW_SRC}/json_ruuvi.c
        ${RUUVI_GW_SRC}/json_ruuvi.h
        ${RUUVI_GW_SRC}/cjson_wrap.c
        ${RUUVI_GW_SRC}/cjson_wrap.h
        ${RUUVI_GW_SRC}/metrics.h
        ${RUUVI_ESP_WRAPPERS_INC}/log.h
        ${WIFI_MANAGER_SRC}/http_server_resp.c
        ${WIFI_MANAGER_SRC}/wifiman_sha256.c
        ${WIFI_MANAGER_SRC}/wifiman_sha256.h
        ${WIFI_MANAGER_SRC}/http_server_auth.c
        ${WIFI_MANAGER_SRC}/http_server_auth.h
        ${WIFI_MANAGER_INC}/http_server_resp.h
        ${WIFI_MANAGER_INC}/wifi_manager.h
        ${WIFI_MANAGER_INC}/wifi_manager_defs.h
        $ENV{IDF_PATH}/components/esp_common/src/esp_err_to_name.c
        $ENV{IDF_PATH}/components/json/cJSON/cJSON.c
        $ENV{IDF_PATH}/components/json/cJSON/cJSON.h
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/sha256.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include/mbedtls/sha256.h
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
        include
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
        $ENV{IDF_PATH}/components/bootloader_support/include
        $ENV{IDF_PATH}/components/esp_http_client/include
        $ENV{IDF_PATH}/components/nghttp/port/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ../../components/esp32-wifi-manager/src/include
        ../../components/esp32-wifi-manager/src
        ../lwip/extra
        ../lwip/lwip/src/include
        ../lwip/lwip/contrib/ports/unix/lib
        ../lwip/lwip/contrib/ports/unix/port/include
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_HTTP_SERVER_CB=1
        CONFIG_DEFAULT_AP_SSID=\"RuuviGateway\"
        GW_GWUI_PARTITION="fatfs_gwui"
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
        ruuvi_esp_wrappers
        ruuvi_esp_wrappers-common_test_funcs
        --coverage
)

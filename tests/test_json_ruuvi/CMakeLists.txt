cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-json_ruuvi)
set(ProjectId ruuvi_gateway_esp-test-json_ruuvi)

add_executable(${ProjectId}
        test_json_ruuvi.cpp
        ${RUUVI_GW_SRC}/json_ruuvi.c
        ${RUUVI_GW_SRC}/json_ruuvi.h
        ${RUUVI_GW_SRC}/cjson_wrap.c
        ${RUUVI_GW_SRC}/cjson_wrap.h
        ${RUUVI_GW_SRC}/gw_cfg.h
        ${RUUVI_GW_SRC}/gw_cfg.c
        ${RUUVI_GW_SRC}/gw_cfg_default.c
        ${RUUVI_GW_SRC}/gw_cfg_default.h
        ${WIFI_MANAGER_SRC}/wifiman_md5.c
        ${WIFI_MANAGER_INC}/wifiman_md5.h
        ${WIFI_MANAGER_SRC}/http_server_auth_type.c
        ${WIFI_MANAGER_INC}/http_server_auth_type.h
        ${RUUVI_ESP_WRAPPERS}/src/mac_addr.c
        ${RUUVI_ESP_WRAPPERS}/include/mac_addr.h
        ${RUUVI_ESP_WRAPPERS}/src/str_buf.c
        ${RUUVI_ESP_WRAPPERS}/include/str_buf.h
        $ENV{IDF_PATH}/components/esp_common/src/esp_err_to_name.c
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
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        $ENV{IDF_PATH}/components/esp_wifi/include
        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/esp_event/include
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_JSON_RUUVI=1
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

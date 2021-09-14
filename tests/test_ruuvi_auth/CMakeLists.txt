cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-ruuvi_auth)
set(ProjectId ruuvi_gateway_esp-test-ruuvi_auth)

add_executable(${ProjectId}
        test_ruuvi_auth.cpp
        ${RUUVI_GW_SRC}/cjson_wrap.c
        ${RUUVI_GW_SRC}/cjson_wrap.h
        ${RUUVI_GW_SRC}/gw_cfg.c
        ${RUUVI_GW_SRC}/gw_cfg.h
        ${RUUVI_GW_SRC}/gw_cfg_default.c
        ${RUUVI_GW_SRC}/gw_cfg_default.h
        ${RUUVI_GW_SRC}/ruuvi_auth.c
        ${RUUVI_GW_SRC}/ruuvi_auth.h
        ${WIFI_MANAGER_SRC}/http_server_auth.c
        ${WIFI_MANAGER_SRC}/http_server_auth.h
        ${WIFI_MANAGER_SRC}/wifiman_md5.c
        ${WIFI_MANAGER_SRC}/include/wifiman_md5.h
        $ENV{IDF_PATH}/components/json/cJSON/cJSON.c
        $ENV{IDF_PATH}/components/json/cJSON/cJSON.h
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/md5.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include/mbedtls/md5.h
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
        ${CMAKE_CURRENT_SOURCE_DIR}
        $ENV{IDF_PATH}/components/json/cJSON
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include
        ../../components/esp32-wifi-manager/src/include
        ../../components/esp32-wifi-manager/src
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_RUUVI_AUTH=1
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
        ruuvi_esp_wrappers
        ruuvi_esp_wrappers-common_test_funcs
        --coverage
)

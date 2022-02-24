cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-gw_cfg_blob)
set(ProjectId ruuvi_gateway_esp-test-gw_cfg_blob)

add_executable(${ProjectId}
        test_gw_cfg_blob.cpp
        ${RUUVI_GW_SRC}/gw_cfg.h
        ${RUUVI_GW_SRC}/gw_cfg_blob.c
        ${RUUVI_GW_SRC}/gw_cfg_blob.h
        ${RUUVI_GW_SRC}/gw_cfg_default.c
        ${RUUVI_GW_SRC}/gw_cfg_default.h
        ${WIFI_MANAGER_SRC}/wifiman_md5.c
        ${WIFI_MANAGER_INC}/wifiman_md5.h
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
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_GW_CFG_BLOB=1
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

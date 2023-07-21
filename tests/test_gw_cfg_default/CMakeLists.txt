cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-gw_cfg_default)
set(ProjectId ruuvi_gateway_esp-test-gw_cfg_default)

add_executable(${ProjectId}
        test_gw_cfg_default.cpp
        ${RUUVI_GW_SRC}/gw_cfg.c
        ${RUUVI_GW_SRC}/gw_cfg.h
        ${RUUVI_GW_SRC}/gw_cfg_default.c
        ${RUUVI_GW_SRC}/gw_cfg_default.h
        ${RUUVI_GW_SRC}/gw_cfg_log.c
        ${RUUVI_GW_SRC}/gw_cfg_log.h
        ${RUUVI_GW_SRC}/event_mgr.h
        ${WIFI_MANAGER_SRC}/http_server_auth_type.c
        ${WIFI_MANAGER_SRC}/wifiman_config.c
        ${WIFI_MANAGER_SRC}/wifiman_config.h
        ${WIFI_MANAGER_SRC}/wifiman_md5.c
        ${WIFI_MANAGER_INC}/wifiman_md5.h
        ${WIFI_MANAGER_SRC}/wifi_manager_default_config.c
        ${WIFI_MANAGER_INC}/wifi_manager.h
        ${RUUVI_ESP_WRAPPERS}/src/os_malloc.c
        ${RUUVI_ESP_WRAPPERS}/include/os_malloc.h
        ${RUUVI_ESP_WRAPPERS}/src/mac_addr.c
        ${RUUVI_ESP_WRAPPERS}/include/mac_addr.h
        ${RUUVI_ESP_WRAPPERS}/src/str_buf.c
        ${RUUVI_ESP_WRAPPERS}/include/str_buf.h
        $ENV{IDF_PATH}/components/lwip/lwip/src/core/ipv4/ip4_addr.c
        $ENV{IDF_PATH}/components/lwip/lwip/src/core/def.c
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
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include
        include
        ${CMAKE_CURRENT_SOURCE_DIR}
        $ENV{IDF_PATH}/components/esp_wifi/include
        $ENV{IDF_PATH}/components/esp_common/include
        ${RUUVI_JSON_STREAM_GEN_INC}
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_GW_CFG_DEFAULT=1
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

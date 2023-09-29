cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-adv_post_cfg_cache)
set(ProjectId ruuvi_gateway_esp-test-adv_post_cfg_cache)

add_executable(${ProjectId}
        test_adv_post_cfg_cache.cpp
        ${RUUVI_GW_SRC}/adv_post_cfg_cache.c
        ${RUUVI_GW_SRC}/adv_post_cfg_cache.h
)

set_target_properties(${ProjectId} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 14
)

target_include_directories(${ProjectId} SYSTEM BEFORE PUBLIC
        include
)

target_include_directories(${ProjectId} PUBLIC
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        ${RUUVI_GW_SRC}
        ${WIFI_MANAGER_INC}
        ${CMAKE_CURRENT_SOURCE_DIR}
        $ENV{IDF_PATH}/components/esp_wifi/include
        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/esp_event/include
        ${RUUVI_JSON_STREAM_GEN_INC}
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_ADV_POST_CFG_CACHE=1
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
#        ruuvi_esp_wrappers
#        ruuvi_esp_wrappers-common_test_funcs
        --coverage
)

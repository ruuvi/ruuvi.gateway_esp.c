cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-metrics)
set(ProjectId ruuvi_gateway_esp-test-metrics)

add_executable(${ProjectId}
        test_metrics.cpp
        ${RUUVI_GW_SRC}/fw_ver.h
        ${RUUVI_GW_SRC}/gw_mac.c
        ${RUUVI_GW_SRC}/gw_mac.h
        ${RUUVI_GW_SRC}/metrics.c
        ${RUUVI_GW_SRC}/metrics.h
        ${RUUVI_ESP_WRAPPERS}/src/str_buf.c
        ${RUUVI_ESP_WRAPPERS}/include/str_buf.h
        ${RUUVI_ESP_WRAPPERS}/src/os_malloc.c
        ${RUUVI_ESP_WRAPPERS}/include/os_malloc.h
        ${RUUVI_ESP_WRAPPERS}/src/mac_addr.c
        ${RUUVI_ESP_WRAPPERS}/include/mac_addr.h
        $ENV{IDF_PATH}/components/json/cJSON/cJSON.c
        $ENV{IDF_PATH}/components/json/cJSON/cJSON.h
)

set_target_properties(${ProjectId} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 14
)

target_include_directories(${ProjectId} PUBLIC
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        include
        $ENV{IDF_PATH}/components/json/cJSON
        $ENV{IDF_PATH}/components/app_update/include
        $ENV{IDF_PATH}/components/spi_flash/include
        $ENV{IDF_PATH}/components/hal/include
        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/bootloader_support/include
        ${RUUVI_GW_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_METRICS=1
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
        ruuvi_esp_wrappers-common_test_funcs
        --coverage
)

cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-nrf52fw)
set(ProjectId ruuvi_gateway_esp-test-nrf52fw)

add_executable(${ProjectId}
        test_nrf52fw.cpp
        crc.cpp
        ${RUUVI_GW_SRC}/nrf52fw.c
        ${RUUVI_GW_SRC}/nrf52fw.h
        ${RUUVI_GW_SRC}/nrf52_fw_ver.c
        ${RUUVI_GW_SRC}/nrf52_fw_ver.h
        ${RUUVI_GW_SRC}/flashfatfs.c
        ${RUUVI_GW_SRC}/flashfatfs.h
        ${RUUVI_ESP_WRAPPERS}/src/os_str.c
        ${RUUVI_ESP_WRAPPERS_INC}/os_str.h
        $ENV{IDF_PATH}/components/esp_common/src/esp_err_to_name.c
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
        ${CMAKE_CURRENT_SOURCE_DIR}
        $ENV{IDF_PATH}/components/esp_rom/include
        $ENV{IDF_PATH}/components/esp_wifi/include
        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/esp_event/include
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_NRF52FW=1
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
        ruuvi_esp_wrappers
        ruuvi_esp_wrappers-common_test_funcs
        --coverage
)

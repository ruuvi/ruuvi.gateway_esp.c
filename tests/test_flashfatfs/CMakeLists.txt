cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-flashfatfs)
set(ProjectId ruuvi_gateway_esp-test-flashfatfs)

add_executable(${ProjectId}
        test_flashfatfs.cpp
        TQueue.hpp
        esp_log_wrapper.cpp
        esp_log_wrapper.hpp
        ../../main/flashfatfs.c
        ../../main/flashfatfs.h
        include/esp_log.h
)

set_target_properties(${ProjectId} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 14
)

target_include_directories(${ProjectId} PUBLIC
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        ../../main
        include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_FLASHFATFS=1
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
        --coverage
)

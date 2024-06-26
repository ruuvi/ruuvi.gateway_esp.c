cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-leds_ctrl2)
set(ProjectId ruuvi_gateway_esp-test-leds_ctrl2)

add_executable(${ProjectId}
        test_leds_ctrl2.cpp
        ${RUUVI_GW_SRC}/leds_ctrl_defs.h
        ${RUUVI_GW_SRC}/leds_ctrl2.c
        ${RUUVI_GW_SRC}/leds_ctrl2.h
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
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_LEDS_CTRL2=1
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

cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-leds_ctrl)
set(ProjectId ruuvi_gateway_esp-test-leds_ctrl)

add_executable(${ProjectId}
        test_leds_ctrl.cpp
        ${RUUVI_GW_SRC}/leds_ctrl.c
        ${RUUVI_GW_SRC}/leds_ctrl.h
        ${RUUVI_GW_SRC}/leds_ctrl2.c
        ${RUUVI_GW_SRC}/leds_ctrl2.h
        ${RUUVI_GW_SRC}/leds_ctrl_defs.h
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
        include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_LEDS_CTRL=1
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

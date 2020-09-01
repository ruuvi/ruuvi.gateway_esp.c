cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-bin2hex)
set(ProjectId ruuvi_gateway_esp-test-bin2hex)

add_executable(${ProjectId}
        test_bin2hex.cpp
        ../../main/bin2hex.c
        ../../main/bin2hex.h
        ../../components/esp32-wifi-manager/src/str_buf.c
        ../../components/esp32-wifi-manager/src/str_buf.h
)

set_target_properties(${ProjectId} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 14
)

target_include_directories(${ProjectId} PUBLIC
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        ../../main
        ../../components/esp32-wifi-manager/src
        include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_BIN2HEX=1
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

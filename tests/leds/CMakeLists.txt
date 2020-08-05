cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-leds)
set(ProjectId ruuvi_gateway_esp-test-leds)

add_executable(${ProjectId}
        test_leds.cpp
        test_events.h
        TQueue.h
        ../../main/leds.c
        ../../main/includes/leds.h
        include/esp_err.h
        include/esp_log.h
        include/esp_timer.h
        driver/ledc.h
)

set_target_properties(${ProjectId} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 17
)

target_include_directories(${ProjectId} PUBLIC
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        ${Posix_GCC_Simulator_SOURCE_DIR}/FreeRTOS_Posix
        ${Posix_GCC_Simulator_SOURCE_DIR}/FreeRTOS_Posix/FreeRTOS_Kernel/include
        ${Posix_GCC_Simulator_SOURCE_DIR}/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix
        ../../main/includes
        ../../src
        include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_LEDS=1
)

target_compile_options(${ProjectId} PUBLIC
        -g3
        -ggdb
        -fprofile-arcs
        -ftest-coverage
        --coverage
)

target_link_options(${ProjectId} PUBLIC
        --coverage
)

target_link_libraries(${ProjectId}
        gtest
        gtest_main
        FreeRTOS_Posix
        gcov
)

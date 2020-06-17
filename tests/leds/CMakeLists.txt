cmake_minimum_required(VERSION 3.5)

project(ruuvi_gateway_esp-test-leds)
set(ProjectId ruuvi_gateway_esp-test-leds)

add_definitions(
        -DRUUVI_TESTS_LEDS=1
)
add_definitions(-g3 -ggdb)

include_directories(
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

set_property(TARGET ${ProjectId} PROPERTY C_STANDARD 99)
set_property(TARGET ${ProjectId} PROPERTY CXX_STANDARD 14)

target_link_libraries(${ProjectId} gtest gtest_main FreeRTOS_Posix)

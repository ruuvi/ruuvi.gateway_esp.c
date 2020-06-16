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
        freertos/src/FreeRTOS_Posix/FreeRTOS_Kernel/include
        freertos/src/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix
        ../../main/includes
        ../../src
        include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set(FREERTOS_SOURCES
        "freertos/src/FreeRTOS_Posix/FreeRTOS_Kernel/croutine.c"
        "freertos/src/FreeRTOS_Posix/FreeRTOS_Kernel/event_groups.c"
        "freertos/src/FreeRTOS_Posix/FreeRTOS_Kernel/list.c"
        "freertos/src/FreeRTOS_Posix/FreeRTOS_Kernel/queue.c"
        "freertos/src/FreeRTOS_Posix/FreeRTOS_Kernel/tasks.c"
        "freertos/src/FreeRTOS_Posix/FreeRTOS_Kernel/timers.c"
        "freertos/src/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix/port.c"
        "freertos/src/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix/portmacro.h"
        "freertos/src/FreeRTOS_Posix/FreeRTOS_Kernel/portable/MemMang/heap_3.c"
)

add_executable(${ProjectId}
        test_leds.cpp
        test_events.h
        TQueue.h
        ../../main/leds.c
        ../../main/includes/leds.h
        include/FreeRTOSConfig.h
        include/esp_err.h
        include/esp_log.h
        include/esp_timer.h
        driver/ledc.h
        ${FREERTOS_SOURCES}
)

set_property(TARGET ${ProjectId} PROPERTY C_STANDARD 99)
set_property(TARGET ${ProjectId} PROPERTY CXX_STANDARD 14)

target_link_libraries(${ProjectId} gtest gtest_main)

cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-leds)
set(ProjectId ruuvi_gateway_esp-test-leds)

add_executable(${ProjectId}
        test_leds.cpp
        test_events.h
        include/esp_timer.h
        driver/ledc.h
        ${RUUVI_GW_SRC}/leds.c
        ${RUUVI_GW_SRC}/leds.h
        ${RUUVI_GW_SRC}/leds_ctrl.c
        ${RUUVI_GW_SRC}/leds_ctrl.h
        ${RUUVI_GW_SRC}/leds_ctrl2.c
        ${RUUVI_GW_SRC}/leds_ctrl2.h
        ${RUUVI_GW_SRC}/leds_blinking.c
        ${RUUVI_GW_SRC}/leds_blinking.h
        $ENV{IDF_PATH}/components/esp_common/src/esp_err_to_name.c
)

set_target_properties(${ProjectId} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 14
)

target_include_directories(${ProjectId} PUBLIC
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        ${Posix_GCC_Simulator_SOURCE_DIR}/FreeRTOS_Posix
        ${Posix_GCC_Simulator_SOURCE_DIR}/FreeRTOS_Posix/FreeRTOS_Kernel/include
        ${Posix_GCC_Simulator_SOURCE_DIR}/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix
        ${RUUVI_GW_SRC}
        include
        $ENV{IDF_PATH}/components/esp_wifi/include
        $ENV{IDF_PATH}/components/tcpip_adapter/include
        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/esp_event/include
        $ENV{IDF_PATH}/components/lwip/include/apps
        ${CMAKE_CURRENT_SOURCE_DIR}
        ../../components/esp32-wifi-manager/src/include
        ${RUUVI_JSON_STREAM_GEN_INC}
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

# CMake has a target_link_options starting from version 3.13
#target_link_options(${ProjectId} PUBLIC
#        --coverage
#)

target_link_libraries(${ProjectId}
        gtest
        gtest_main
        FreeRTOS_Posix
        gcov
        ruuvi_esp_wrappers
        ruuvi_esp_wrappers-common_test_funcs
        esp_simul
        --coverage
)

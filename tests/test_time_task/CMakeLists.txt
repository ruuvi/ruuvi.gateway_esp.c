cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-time_task)
set(ProjectId ruuvi_gateway_esp-test-time_task)

add_executable(${ProjectId}
        test_time_task.cpp
        test_events.hpp
        ${RUUVI_GW_SRC}/time_task.c
        ${RUUVI_GW_SRC}/includes/time_task.h
        ${RUUVI_GW_SRC}/event_mgr.c
        ${RUUVI_GW_SRC}/event_mgr.h
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
        $ENV{IDF_PATH}/components/lwip/include/apps
        $ENV{IDF_PATH}/components/lwip/include/apps/sntp
        ${RUUVI_GW_SRC}/includes
        ${RUUVI_GW_SRC}
        include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_TIME_TASK=1
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
        lwip
        --coverage
)

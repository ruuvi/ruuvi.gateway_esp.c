cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-os_task)
set(ProjectId ruuvi_gateway_esp-test-os_task)

add_executable(${ProjectId}
        test_os_task.cpp
        ../../main/os_task.c
        ../../main/os_task.h
        ../../main/log.h
        ../common/include/esp_log.h
        ../common/include/esp_err.h
        ../common/TQueue.hpp
        ../common/esp_log_wrapper.cpp
        ../common/esp_log_wrapper.hpp
        $ENV{IDF_PATH}/components/esp_common/src/esp_err_to_name.c
)

set_target_properties(${ProjectId} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 14
)

target_include_directories(${ProjectId} PUBLIC
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        ../common
        ../common/include
        ../../main
        ${CMAKE_CURRENT_SOURCE_DIR}
        ../Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include
        ../Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix
        ../Posix_GCC_Simulator/FreeRTOS_Posix
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_OS_TASK=1
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

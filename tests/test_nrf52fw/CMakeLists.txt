cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-nrf52fw)
set(ProjectId ruuvi_gateway_esp-test-nrf52fw)

add_executable(${ProjectId}
        test_nrf52fw.cpp
        TQueue.hpp
        esp_log_wrapper.cpp
        esp_log_wrapper.hpp
        crc.cpp
        ../../main/app_wrappers.c
        ../../main/app_wrappers.h
        ../../main/nrf52fw.c
        ../../main/nrf52fw.h
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
        $ENV{IDF_PATH}/components/esp_rom/include
        ../Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include
        ../Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix
        ../Posix_GCC_Simulator/FreeRTOS_Posix
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_NRF52FW=1
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

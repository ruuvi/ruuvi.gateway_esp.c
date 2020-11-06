cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-gw_cfg)
set(ProjectId ruuvi_gateway_esp-test-gw_cfg)

add_executable(${ProjectId}
        test_gw_cfg.cpp
        ../../main/gw_cfg.c
        ../../main/gw_cfg.h
        ../../main/cjson_wrap.c
        ../../main/cjson_wrap.h
        ../../main/log.h
        ../common/include/esp_log.h
        ../common/include/esp_err.h
        ../common/TQueue.hpp
        ../common/esp_log_wrapper.cpp
        ../common/esp_log_wrapper.hpp
        $ENV{IDF_PATH}/components/esp_common/src/esp_err_to_name.c
        $ENV{IDF_PATH}/components/json/cJSON/cJSON.c
        $ENV{IDF_PATH}/components/json/cJSON/cJSON.h
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
        include
        $ENV{IDF_PATH}/components/json/cJSON
        ${CMAKE_CURRENT_SOURCE_DIR}

        ../Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include
        ../Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix
        ../Posix_GCC_Simulator/FreeRTOS_Posix
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_GW_CFG=1
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

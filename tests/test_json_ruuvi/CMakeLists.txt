cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-json_ruuvi)
set(ProjectId ruuvi_gateway_esp-test-json_ruuvi)

add_executable(${ProjectId}
        test_json_ruuvi.cpp
        ${RUUVI_GW_SRC}/json_ruuvi.c
        ${RUUVI_GW_SRC}/json_ruuvi.h
        ${RUUVI_GW_SRC}/cjson_wrap.c
        ${RUUVI_GW_SRC}/cjson_wrap.h
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
        ${RUUVI_GW_SRC}
        include
        $ENV{IDF_PATH}/components/json/cJSON
        ${CMAKE_CURRENT_SOURCE_DIR}

        ../Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include
        ../Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix
        ../Posix_GCC_Simulator/FreeRTOS_Posix
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_JSON_RUUVI=1
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
        ruuvi_esp_wrappers
        ruuvi_esp_wrappers-common_test_funcs
        --coverage
)

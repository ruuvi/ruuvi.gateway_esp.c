cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-nrf52swd)
set(ProjectId ruuvi_gateway_esp-test-nrf52swd)

add_executable(${ProjectId}
        test_nrf52swd.cpp
        ${RUUVI_GW_SRC}/nrf52swd.c
        ${RUUVI_GW_SRC}/nrf52swd.h
        $ENV{IDF_PATH}/components/esp_common/src/esp_err_to_name.c
)

set_target_properties(${ProjectId} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 14
)

target_include_directories(${ProjectId} PUBLIC
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        ${RUUVI_GW_SRC}
        ../../components/libSWD-esp32/include
        ../../components/ruuvi.boards.c
        include
        ${CMAKE_CURRENT_SOURCE_DIR}
        $ENV{IDF_PATH}/components/driver/include
        $ENV{IDF_PATH}/components/soc/include
        $ENV{IDF_PATH}/components/soc/esp32/include

        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/esp_rom/include
        $ENV{IDF_PATH}/components/esp32/include
        $ENV{IDF_PATH}/components/soc/soc/esp32/include
        $ENV{IDF_PATH}/components/soc/soc/include
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_NRF52SWD=1
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
        ruuvi_esp_wrappers-common_test_funcs
        --coverage
)

cmake_minimum_required(VERSION 3.13)

project(ruuvi_gateway_esp-test-gw_cfg_storage)
set(ProjectId ruuvi_gateway_esp-test-gw_cfg_storage)

add_executable(${ProjectId}
        test_gw_cfg_storage.cpp
        ${RUUVI_GW_SRC}/gw_cfg_storage.c
        ${RUUVI_GW_SRC}/gw_cfg_storage_files.c
        ${RUUVI_GW_SRC}/gw_cfg_storage.h
        ${RUUVI_ESP_WRAPPERS}/src/str_buf.c
        ${RUUVI_ESP_WRAPPERS_INC}/str_buf.h
        ${RUUVI_ESP_WRAPPERS}/src/snprintf_with_esp_err_desc.c
        ${RUUVI_ESP_WRAPPERS}/include/snprintf_with_esp_err_desc.h
        ${RUUVI_ESP_WRAPPERS}/include/wrap_esp_err_to_name_r.h
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
        ../include
        ${COMPONENTS}/nvs_flash/include
        ${COMPONENTS}/spi_flash/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/spi_flash/include
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_GW_CFG_STORAGE=1
)

target_compile_options(${ProjectId} PUBLIC
        -g3
        -ggdb
        -fprofile-arcs
        -ftest-coverage
)

target_link_options(${ProjectId} PUBLIC
        --coverage
)

target_link_libraries(${ProjectId}
        gtest
        gtest_main
        gcov
        ruuvi_esp_wrappers-common_test_funcs
)

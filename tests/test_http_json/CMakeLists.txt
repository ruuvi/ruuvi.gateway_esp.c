cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-http_json)
set(ProjectId ruuvi_gateway_esp-test-http_json)

add_executable(${ProjectId}
        test_http_json.cpp
        ${RUUVI_GW_SRC}/http_json.c
        ${RUUVI_GW_SRC}/http_json.h
        ${RUUVI_GW_SRC}/bin2hex.c
        ${RUUVI_GW_SRC}/bin2hex.h
        ${RUUVI_GW_SRC}/cjson_wrap.c
        ${RUUVI_GW_SRC}/cjson_wrap.h
        ${RUUVI_GW_SRC}/mac_addr.c
        ${RUUVI_GW_SRC}/mac_addr.h
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
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_HTTP_JSON=1
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

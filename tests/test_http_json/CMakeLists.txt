cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-http_json)
set(ProjectId ruuvi_gateway_esp-test-http_json)

add_executable(${ProjectId}
        test_http_json.cpp
        ${RUUVI_GW_SRC}/http_json.c
        ${RUUVI_GW_SRC}/http_json.h
        ${RUUVI_GW_SRC}/adv_decode_0x05.c
        ${RUUVI_GW_SRC}/adv_decode_0x06.c
        ${RUUVI_GW_SRC}/adv_decode_0xe0.c
        ${RUUVI_GW_SRC}/adv_decode_0xe1.c
        ${RUUVI_GW_SRC}/adv_decode_0xf0.c
        ${RUUVI_GW_SRC}/adv_decode.h
        ${RUUVI_GW_SRC}/cjson_wrap.c
        ${RUUVI_GW_SRC}/cjson_wrap.h
        ${RUUVI_JSON_STREAM_GEN_SRC}/json_stream_gen.c
        ${RUUVI_JSON_STREAM_GEN_INC}/json_stream_gen.h
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_5.c
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_5.h
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_6.c
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_6.h
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_f0.c
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_f0.h
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_e0.c
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_e0.h
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_e1.c
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoint_e1.h
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoints.c
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoints.h
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
        ${WIFI_MANAGER_INC}
        include
        ${COMPONENTS}/bootloader_support/include
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src
        $ENV{IDF_PATH}/components/json/cJSON
        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/app_update/include
        $ENV{IDF_PATH}/components/spi_flash/include
        $ENV{IDF_PATH}/components/soc/include
        $ENV{IDF_PATH}/components/esp_wifi/include
        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/esp_event/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${RUUVI_JSON_STREAM_GEN_INC}
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

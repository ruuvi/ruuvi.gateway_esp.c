cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-mqtt_json)
set(ProjectId ruuvi_gateway_esp-test-mqtt_json)

add_executable(${ProjectId}
        test_mqtt_json.cpp
        ${RUUVI_GW_SRC}/mqtt_json.c
        ${RUUVI_GW_SRC}/mqtt_json.h
        ${RUUVI_GW_SRC}/adv_decode_0x05.c
        ${RUUVI_GW_SRC}/adv_decode_0x06.c
        ${RUUVI_GW_SRC}/adv_decode_0xe0.c
        ${RUUVI_GW_SRC}/adv_decode_0xf0.c
        ${RUUVI_GW_SRC}/adv_decode.h
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
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoints.c
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src/ruuvi_endpoints.h
        ${RUUVI_ESP_WRAPPERS}/src/str_buf.c
        ${RUUVI_ESP_WRAPPERS_INC}/str_buf.h
        ${RUUVI_ESP_WRAPPERS}/src/mac_addr.c
        ${RUUVI_ESP_WRAPPERS_INC}/mac_addr.h
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
        ${WIFI_MANAGER_INC}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${COMPONENTS}/ruuvi.comm_tester.c/components/ruuvi.endpoints.c/src
        ${RUUVI_JSON_STREAM_GEN_INC}
        $ENV{IDF_PATH}/components/esp_wifi/include
        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/esp_event/include
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_MQTT_JSON=1
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
#        ruuvi_esp_wrappers
        ruuvi_esp_wrappers-common_test_funcs
        --coverage
)

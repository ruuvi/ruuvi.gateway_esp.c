cmake_minimum_required(VERSION 3.22)

project(ruuvi_gateway_esp-test-http_post_event_handler)
set(ProjectId ruuvi_gateway_esp-test-http_post_event_handler)

add_executable(${ProjectId}
        test_http_post_event_handler.cpp
        ${RUUVI_GW_SRC}/http_post_event_handler.c
        ${RUUVI_GW_SRC}/http_post_event_handler.h
        ${RUUVI_ESP_WRAPPERS_INC}/str_buf.h
        ${RUUVI_ESP_WRAPPERS}/src/str_buf.c
        ${RUUVI_ESP_WRAPPERS}/src/os_malloc.c
        ${RUUVI_ESP_WRAPPERS_INC}/os_malloc.h
        ${RUUVI_ESP_WRAPPERS}/src/os_str.c
        ${RUUVI_ESP_WRAPPERS_INC}/os_str.h
)

set_target_properties(${ProjectId} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 17
)

target_include_directories(${ProjectId} PUBLIC
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        ${RUUVI_GW_SRC}
        include
        ../include
        ${COMPONENTS}/esp_http_client/include
        ${COMPONENTS}/mbedtls/mbedtls/include
        ${COMPONENTS}/mbedtls/mbedtls/library
        ${COMPONENTS}/esp-tls
        ${COMPONENTS}/esp32-wifi-manager/src/include
        ${COMPONENTS}/nvs_flash/include
        $ENV{IDF_PATH}/components/json/cJSON
        $ENV{IDF_PATH}/components/nghttp/port/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${RUUVI_JSON_STREAM_GEN_INC}
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_HTTP_POST_EVENT_HANDLER=1
        OS_MALLOC_TRACE=1
        OS_MALLOC_TRACE_DISABLE_TIMESTAMP=1
)

target_compile_options(${ProjectId} PUBLIC
        -g3
        -ggdb
        -fprofile-arcs
        -ftest-coverage
        --coverage
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

target_link_options(${ProjectId} PRIVATE
        "-Wl,--wrap=malloc"
        "-Wl,--wrap=calloc"
        "-Wl,--wrap=free"
)



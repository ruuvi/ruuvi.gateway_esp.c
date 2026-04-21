cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-esp_http_client)
set(ProjectId ruuvi_gateway_esp-test-esp_http_client)

add_executable(${ProjectId}
        test_esp_http_client.cpp
        tcp_transport_stub.c
        tcp_transport_stub.h
        http_event.hpp
        ${COMPONENTS}/esp_http_client/esp_http_client.c
        ${COMPONENTS}/esp_http_client/include/esp_http_client.h
        ${COMPONENTS}/esp_http_client/lib/http_auth.c
        ${COMPONENTS}/esp_http_client/lib/http_header.c
        ${COMPONENTS}/esp_http_client/lib/http_utils.c
        ${COMPONENTS}/esp_http_client/lib/include/http_auth.h
        ${COMPONENTS}/esp_http_client/lib/include/http_header.h
        ${COMPONENTS}/esp_http_client/lib/include/http_utils.h
        ${COMPONENTS}/esp-tls/esp-tls-crypto/esp_tls_crypto.c
        ${COMPONENTS}/mbedtls/mbedtls/library/base64.c
        ${COMPONENTS}/mbedtls/mbedtls/library/md5.c
        ${COMPONENTS}/mbedtls/mbedtls/library/platform_util.c
        ${COMPONENTS}/mbedtls/mbedtls/library/sha1.c
        ${COMPONENTS}/tcp_transport/transport.c
        ${COMPONENTS}/esp_common/src/esp_err_to_name.c
        $ENV{IDF_PATH}/components/nghttp/port/http_parser.c
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
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${COMPONENTS}/esp_http_client/include
        ${COMPONENTS}/esp_http_client/lib/include
        ${COMPONENTS}/tcp_transport/include
        ${COMPONENTS}/mbedtls/esp_crt_bundle/include
        ${COMPONENTS}/mbedtls/mbedtls/include
        ${COMPONENTS}/esp-tls
        ${COMPONENTS}/esp-tls/esp-tls-crypto
        ${COMPONENTS}/tcp_transport/private_include
        $ENV{IDF_PATH}/components/log/include
        $ENV{IDF_PATH}/components/esp_rom/include
        $ENV{IDF_PATH}/components/esp_wifi/include
        $ENV{IDF_PATH}/components/tcpip_adapter/include
        $ENV{IDF_PATH}/components/esp_event/include
        $ENV{IDF_PATH}/components/lwip/include/apps
        $ENV{IDF_PATH}/components/soc/include
        $ENV{IDF_PATH}/components/nghttp/port/include
        $ENV{IDF_PATH}/components/esp_common/include
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_ESP_HTTP_CLIENT=1
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

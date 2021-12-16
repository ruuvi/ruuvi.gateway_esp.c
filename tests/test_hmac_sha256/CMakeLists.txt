cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-hmac_sha256)
set(ProjectId ruuvi_gateway_esp-test-hmac_sha256)

add_executable(${ProjectId}
        test_hmac_sha256.cpp
        ${RUUVI_GW_SRC}/hmac_sha256.c
        ${RUUVI_GW_SRC}/hmac_sha256.h
        ${RUUVI_GW_SRC}/ruuvi_device_id.c
        ${RUUVI_GW_SRC}/ruuvi_device_id.h
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/md_wrap.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/md.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/md5.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/platform_util.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/ripemd160.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/sha1.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/sha256.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/sha512.c
        $ENV{IDF_PATH}/components/mbedtls/port/include/mbedtls/esp_config.h
)

set_target_properties(${ProjectId} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 14
)

target_include_directories(${ProjectId} PUBLIC
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        ${RUUVI_GW_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_HMAC_SHA256=1
        MBEDTLS_PLATFORM_MEMORY=1
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

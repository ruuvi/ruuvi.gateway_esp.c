cmake_minimum_required(VERSION 3.7)

project(ruuvi_gateway_esp-test-http_server_cb)
set(ProjectId ruuvi_gateway_esp-test-http_server_cb)

add_executable(${ProjectId}
        test_http_server_cb.cpp
        ${RUUVI_GW_SRC}/gw_cfg.c
        ${RUUVI_GW_SRC}/gw_cfg.h
        ${RUUVI_GW_SRC}/http_server_cb.c
        ${RUUVI_GW_SRC}/http_server_cb.h
        ${RUUVI_GW_SRC}/json_ruuvi.c
        ${RUUVI_GW_SRC}/json_ruuvi.h
        ${RUUVI_GW_SRC}/cjson_wrap.c
        ${RUUVI_GW_SRC}/cjson_wrap.h
        ${RUUVI_ESP_WRAPPERS_INC}/log.h
        ${WIFI_MANAGER_SRC}/http_server_resp.c
        ${WIFI_MANAGER_INC}/http_server_resp.h
        ${WIFI_MANAGER_INC}/wifi_manager.h
        ${WIFI_MANAGER_INC}/wifi_manager_defs.h
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
        ${RUUVI_GW_SRC}/includes
        include
        $ENV{IDF_PATH}/components/json/cJSON
        $ENV{IDF_PATH}/components/esp_wifi/include
        $ENV{IDF_PATH}/components/tcpip_adapter/include
        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/esp_event/include
        $ENV{IDF_PATH}/components/lwip/include/apps
        ${CMAKE_CURRENT_SOURCE_DIR}
        ../../components/esp32-wifi-manager/src/include
        ../../components/esp32-wifi-manager/src
        ../../components/esp32-wifi-manager/tests/lwip/extra
        ../../components/esp32-wifi-manager/tests/lwip/lwip/src/include
        ../../components/esp32-wifi-manager/tests/lwip/lwip/contrib/ports/unix/lib
        ../../components/esp32-wifi-manager/tests/lwip/lwip/contrib/ports/unix/port/include
        ../Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include
        ../Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix
        ../Posix_GCC_Simulator/FreeRTOS_Posix
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_HTTP_SERVER_CB=1
        GW_GWUI_PARTITION="fatfs_gwui"
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

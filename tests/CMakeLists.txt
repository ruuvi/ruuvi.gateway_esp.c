cmake_minimum_required(VERSION 3.5)

project(ruuvi_gateway_esp-tests)

add_definitions(
        -DRUUVI_TESTS=1
)

set(RUUVI_ESP_WRAPPERS ${CMAKE_CURRENT_SOURCE_DIR}/../components/ruuvi.esp_wrappers.c)
set(RUUVI_ESP_WRAPPERS_INC ${RUUVI_ESP_WRAPPERS}/include)

set(RUUVI_ESP_WRAPPERS_TESTS_COMMON ${RUUVI_ESP_WRAPPERS}/tests/common)
set(RUUVI_ESP_WRAPPERS_TESTS_COMMON_INC ${RUUVI_ESP_WRAPPERS_TESTS_COMMON}/include)

include_directories(
        ${RUUVI_ESP_WRAPPERS_INC}
        ${RUUVI_ESP_WRAPPERS_TESTS_COMMON_INC}
)

set(RUUVI_GW_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../main)

set(WIFI_MANAGER ${CMAKE_CURRENT_SOURCE_DIR}/../components/esp32-wifi-manager)
set(WIFI_MANAGER_SRC ${WIFI_MANAGER}/src)
set(WIFI_MANAGER_INC ${WIFI_MANAGER}/src)

enable_testing()
add_subdirectory(googletest)
add_subdirectory(Posix_GCC_Simulator)

include_directories(
        "Posix_GCC_Simulator/FreeRTOS_Posix"
        "Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include"
        "Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include/freertos"
        "Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix"
)

add_subdirectory(${RUUVI_ESP_WRAPPERS} ruuvi.esp_wrappers.c)
add_subdirectory(${RUUVI_ESP_WRAPPERS_TESTS_COMMON} ruuvi.esp_wrappers_tests)

add_subdirectory(test_bin2hex)
add_subdirectory(test_cjson_wrap)
add_subdirectory(test_flashfatfs)
add_subdirectory(test_json_ruuvi)
add_subdirectory(test_gw_cfg)
add_subdirectory(test_http_server_cb)
add_subdirectory(test_leds)
add_subdirectory(test_mac_addr)
add_subdirectory(test_nrf52fw)
add_subdirectory(test_nrf52swd)

add_test(NAME test_bin2hex
        COMMAND ruuvi_gateway_esp-test-bin2hex
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-bin2hex>/gtestresults.xml
)

add_test(NAME test_cjson_wrap
        COMMAND ruuvi_gateway_esp-test-cjson_wrap
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-cjson_wrap>/gtestresults.xml
)

add_test(NAME test_flashfatfs
        COMMAND ruuvi_gateway_esp-test-flashfatfs
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-flashfatfs>/gtestresults.xml
        )

add_test(NAME test_json_ruuvi
        COMMAND ruuvi_gateway_esp-test-json_ruuvi
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-json_ruuvi>/gtestresults.xml
        )

add_test(NAME test_gw_cfg
        COMMAND ruuvi_gateway_esp-test-gw_cfg
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-gw_cfg>/gtestresults.xml
        )

add_test(NAME test_http_server_cb
        COMMAND ruuvi_gateway_esp-test-http_server_cb
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-http_server_cb>/gtestresults.xml
)

add_test(NAME test_leds
        COMMAND ruuvi_gateway_esp-test-leds
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-leds>/gtestresults.xml
)

add_test(NAME test_mac_addr
        COMMAND ruuvi_gateway_esp-test-mac_addr
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-mac_addr>/gtestresults.xml
)

add_test(NAME test_nrf52fw
        COMMAND ruuvi_gateway_esp-test-nrf52fw
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-nrf52fw>/gtestresults.xml
)

add_test(NAME test_nrf52swd
        COMMAND ruuvi_gateway_esp-test-nrf52swd
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-nrf52swd>/gtestresults.xml
)

cmake_minimum_required(VERSION 3.5)

project(ruuvi_gateway_esp-tests)

add_definitions(
        -DRUUVI_TESTS=1
)

enable_testing()
add_subdirectory(googletest)
add_subdirectory(Posix_GCC_Simulator)

add_subdirectory(test_app_malloc)
add_subdirectory(test_app_wrappers)
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
add_subdirectory(test_os_task)
add_subdirectory(test_time_units)

add_test(NAME test_app_malloc
        COMMAND ruuvi_gateway_esp-test-app_malloc
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-app_malloc>/gtestresults.xml
)

add_test(NAME test_app_wrappers
        COMMAND ruuvi_gateway_esp-test-app_wrappers
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-app_wrappers>/gtestresults.xml
)

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

add_test(NAME test_os_task
        COMMAND ruuvi_gateway_esp-test-os_task
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-os_task>/gtestresults.xml
)

add_test(NAME test_time_units
        COMMAND ruuvi_gateway_esp-test-time_units
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-time_units>/gtestresults.xml
)

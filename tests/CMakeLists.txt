cmake_minimum_required(VERSION 3.5)

project(ruuvi_gateway_esp-tests)

add_definitions(
        -DRUUVI_TESTS=1
        -D_GNU_SOURCE
)

set(COMPONENTS ${CMAKE_CURRENT_SOURCE_DIR}/../components)

set(RUUVI_ESP_WRAPPERS ${COMPONENTS}/ruuvi.esp_wrappers.c)
set(RUUVI_ESP_WRAPPERS_INC ${RUUVI_ESP_WRAPPERS}/include)

set(RUUVI_ESP_WRAPPERS_TESTS_COMMON ${RUUVI_ESP_WRAPPERS}/tests/common)
set(RUUVI_ESP_WRAPPERS_TESTS_COMMON_INC ${RUUVI_ESP_WRAPPERS_TESTS_COMMON}/include)

set(RUUVI_JSON_STREAM_GEN ${COMPONENTS}/ruuvi.json_stream_gen.c)
set(RUUVI_JSON_STREAM_GEN_SRC ${RUUVI_JSON_STREAM_GEN}/src)
set(RUUVI_JSON_STREAM_GEN_INC ${RUUVI_JSON_STREAM_GEN}/include)

include_directories(
        ${RUUVI_ESP_WRAPPERS_INC}
        ${RUUVI_ESP_WRAPPERS_TESTS_COMMON_INC}
)

set(RUUVI_GW_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../main)

set(WIFI_MANAGER ${CMAKE_CURRENT_SOURCE_DIR}/../components/esp32-wifi-manager)
set(WIFI_MANAGER_SRC ${WIFI_MANAGER}/src)
set(WIFI_MANAGER_INC ${WIFI_MANAGER}/src/include)

enable_testing()
add_subdirectory(googletest)
add_subdirectory(posix_gcc_simulator)

include_directories(
        "posix_gcc_simulator/FreeRTOS_Posix"
        "posix_gcc_simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include"
        "posix_gcc_simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include/freertos"
        "posix_gcc_simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix"
)

add_subdirectory(${RUUVI_ESP_WRAPPERS} ruuvi.esp_wrappers.c)
add_subdirectory(${RUUVI_ESP_WRAPPERS_TESTS_COMMON} ruuvi.esp_wrappers_tests)

add_subdirectory(esp_simul)

add_subdirectory(lwip)

include_directories(
        "lwip/lwip/src/include"
        "lwip/extra"
        "lwip/lwip/contrib/ports/freertos/include"
        "lwip/lwip/contrib/ports/unix/port/include"
)

add_subdirectory(test_adv_table)
add_subdirectory(test_bin2hex)
add_subdirectory(test_cjson_wrap)
add_subdirectory(test_event_mgr)
add_subdirectory(test_flashfatfs)
add_subdirectory(test_json_ruuvi)
add_subdirectory(test_gw_cfg)
add_subdirectory(test_gw_cfg_blob)
add_subdirectory(test_gw_cfg_default)
add_subdirectory(test_gw_cfg_json)
add_subdirectory(test_gw_cfg_ruuvi_json_generate)
add_subdirectory(test_hmac_sha256)
add_subdirectory(test_http_json)
add_subdirectory(test_http_server_cb)
add_subdirectory(test_leds)
add_subdirectory(test_leds_blinking)
add_subdirectory(test_leds_ctrl)
add_subdirectory(test_leds_ctrl2)
add_subdirectory(test_metrics)
add_subdirectory(test_mqtt_json)
add_subdirectory(test_nrf52fw)
add_subdirectory(test_nrf52swd)
add_subdirectory(test_ruuvi_auth)
add_subdirectory(test_time_str)
add_subdirectory(test_time_task)
add_subdirectory(test_url_encode)

add_test(NAME test_adv_table
        COMMAND ruuvi_gateway_esp-test-adv_table
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-adv_table>/gtestresults.xml
)

add_test(NAME test_bin2hex
        COMMAND ruuvi_gateway_esp-test-bin2hex
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-bin2hex>/gtestresults.xml
)

add_test(NAME test_cjson_wrap
        COMMAND ruuvi_gateway_esp-test-cjson_wrap
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-cjson_wrap>/gtestresults.xml
)

add_test(NAME test_event_mgr
        COMMAND ruuvi_gateway_esp-test-event_mgr
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-event_mgr>/gtestresults.xml
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

add_test(NAME test_gw_cfg_json
        COMMAND ruuvi_gateway_esp-test-gw_cfg_json
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-gw_cfg_json>/gtestresults.xml
)

add_test(NAME test_gw_cfg_blob
        COMMAND ruuvi_gateway_esp-test-gw_cfg_blob
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-gw_cfg_blob>/gtestresults.xml
)

add_test(NAME test_gw_cfg_default
        COMMAND ruuvi_gateway_esp-test-gw_cfg_default
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-gw_cfg_default>/gtestresults.xml
)

add_test(NAME test_gw_cfg_ruuvi_json_generate
        COMMAND ruuvi_gateway_esp-test-gw_cfg_ruuvi_json_generate
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-gw_cfg_ruuvi_json_generate>/gtestresults.xml
)

add_test(NAME test_hmac_sha256
        COMMAND ruuvi_gateway_esp-test-hmac_sha256
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-hmac_sha256>/gtestresults.xml
)

add_test(NAME test_http_json
        COMMAND ruuvi_gateway_esp-test-http_json
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-http_json>/gtestresults.xml
)

add_test(NAME test_http_server_cb
        COMMAND ruuvi_gateway_esp-test-http_server_cb
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-http_server_cb>/gtestresults.xml
)

#add_test(NAME test_leds
#        COMMAND ruuvi_gateway_esp-test-leds
#            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-leds>/gtestresults.xml
#)

add_test(NAME test_leds_blinking
        COMMAND ruuvi_gateway_esp-test-leds_blinking
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-leds_blinking>/gtestresults.xml
)

add_test(NAME test_leds_ctrl
        COMMAND ruuvi_gateway_esp-test-leds_ctrl
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-leds_ctrl>/gtestresults.xml
)

add_test(NAME test_leds_ctrl2
        COMMAND ruuvi_gateway_esp-test-leds_ctrl2
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-leds_ctrl2>/gtestresults.xml
)

add_test(NAME test_metrics
        COMMAND ruuvi_gateway_esp-test-metrics
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-metrics>/gtestresults.xml
)

add_test(NAME test_mqtt_json
        COMMAND ruuvi_gateway_esp-test-mqtt_json
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-mqtt_json>/gtestresults.xml
)

add_test(NAME test_nrf52fw
        COMMAND ruuvi_gateway_esp-test-nrf52fw
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-nrf52fw>/gtestresults.xml
)

add_test(NAME test_nrf52swd
        COMMAND ruuvi_gateway_esp-test-nrf52swd
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-nrf52swd>/gtestresults.xml
)

add_test(NAME test_ruuvi_auth
        COMMAND ruuvi_gateway_esp-test-ruuvi_auth
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-ruuvi_auth>/gtestresults.xml
)

add_test(NAME test_time_str
        COMMAND ruuvi_gateway_esp-test-time_str
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-time_str>/gtestresults.xml
)

#add_test(NAME test_time_task
#        COMMAND ruuvi_gateway_esp-test-time_task
#            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-time_task>/gtestresults.xml
#)

add_test(NAME test_url_encode
        COMMAND ruuvi_gateway_esp-test-url_encode
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-url_encode>/gtestresults.xml
)


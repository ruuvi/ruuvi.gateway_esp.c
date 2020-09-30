cmake_minimum_required(VERSION 3.5)

project(ruuvi_gateway_esp-tests)

add_definitions(
        -DRUUVI_TESTS=1
)

enable_testing()
add_subdirectory(googletest)
add_subdirectory(Posix_GCC_Simulator)
add_subdirectory(test_bin2hex)
add_subdirectory(test_leds)
add_subdirectory(test_nrf52fw)

add_test(NAME test_bin2hex
        COMMAND ruuvi_gateway_esp-test-bin2hex
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-bin2hex>/gtestresults.xml
)

add_test(NAME test_leds
        COMMAND ruuvi_gateway_esp-test-leds
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-leds>/gtestresults.xml
)

add_test(NAME test_nrf52fw
        COMMAND ruuvi_gateway_esp-test-nrf52fw
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-nrf52fw>/gtestresults.xml
)

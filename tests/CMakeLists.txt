cmake_minimum_required(VERSION 3.5)

project(ruuvi_gateway_esp-tests)

add_definitions(
        -DRUUVI_TESTS=1
)

enable_testing()
add_subdirectory(googletest)
add_subdirectory(Posix_GCC_Simulator)
add_subdirectory(leds)

add_test(NAME leds
        COMMAND ruuvi_gateway_esp-test-leds
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_gateway_esp-test-leds>/gtestresults.xml
)

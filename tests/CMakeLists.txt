cmake_minimum_required(VERSION 3.5)

project(ruuvi_gateway_esp-tests)

add_definitions(
        -DRUUVI_TESTS=1
)

enable_testing()
add_subdirectory(googletest)
add_subdirectory(leds)


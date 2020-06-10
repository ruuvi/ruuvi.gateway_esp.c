
project(ruuvi_gateway_esp-test-leds)
set(ProjectId ruuvi_gateway_esp-test-leds)

add_definitions(
        -DRUUVI_TESTS_LEDS=1
)

include_directories(
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        ../../src
)

add_executable(${ProjectId}
        test1.cpp
#        ../../main/leds.c
#        ../../main/includes/leds.h
)

set_property(TARGET ${ProjectId} PROPERTY C_STANDARD 99)
set_property(TARGET ${ProjectId} PROPERTY CXX_STANDARD 14)

target_link_libraries(${ProjectId} gtest gtest_main)

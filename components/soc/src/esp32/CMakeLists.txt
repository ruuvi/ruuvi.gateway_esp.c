set(ORIG_SOC_SRC_ESP32 $ENV{IDF_PATH}/components/soc/src/esp32)

set(srcs "${ORIG_SOC_SRC_ESP32}/adc_hal.c"
    "${ORIG_SOC_SRC_ESP32}/brownout_hal.c"
    "${ORIG_SOC_SRC_ESP32}/rtc_clk.c"
    "${ORIG_SOC_SRC_ESP32}/rtc_clk_init.c"
    "${ORIG_SOC_SRC_ESP32}/rtc_init.c"
    "${ORIG_SOC_SRC_ESP32}/rtc_pm.c"
    "${ORIG_SOC_SRC_ESP32}/rtc_sleep.c"
    "${ORIG_SOC_SRC_ESP32}/rtc_time.c"
    "${ORIG_SOC_SRC_ESP32}/rtc_wdt.c"
    "${ORIG_SOC_SRC_ESP32}/soc_memory_layout.c"
    "${ORIG_SOC_SRC_ESP32}/touch_sensor_hal.c")

if(NOT BOOTLOADER_BUILD AND CONFIG_ETH_USE_ESP32_EMAC)
    list(APPEND srcs "${CMAKE_CURRENT_LIST_DIR}/emac_hal.c")
endif()

target_sources(${COMPONENT_LIB} PRIVATE "${srcs}")
target_include_directories(${COMPONENT_LIB} PUBLIC ${ORIG_SOC_SRC_ESP32} ${ORIG_SOC_SRC_ESP32}/include)
target_include_directories(${COMPONENT_LIB} PRIVATE ${ORIG_SOC_SRC_ESP32}/../hal)

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    set_source_files_properties("${ORIG_SOC_SRC_ESP32}/rtc_clk.c" PROPERTIES
                                COMPILE_FLAGS "-fno-jump-tables -fno-tree-switch-conversion")
endif()

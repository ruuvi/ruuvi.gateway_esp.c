idf_build_get_property(target IDF_TARGET)

idf_build_get_property(idf_path IDF_PATH)
if(NOT idf_path)
    message(FATAL_ERROR "IDF_PATH is not set or could not be determined by idf_build_get_property(IDF_PATH)")
endif()
set(ORIG_SOC "${idf_path}/components/soc")

set(srcs "${ORIG_SOC}/src/cpu_util.c"
          "${ORIG_SOC}/src/memory_layout_utils.c"
          "${ORIG_SOC}/src/lldesc.c"
          "${ORIG_SOC}/src/hal/cpu_hal.c"
          "${ORIG_SOC}/src/hal/rmt_hal.c"
          "${ORIG_SOC}/src/hal/rtc_io_hal.c"
          "${ORIG_SOC}/src/hal/dac_hal.c"
          "${ORIG_SOC}/src/hal/adc_hal.c"
          "${ORIG_SOC}/src/hal/spi_hal.c"
          "${ORIG_SOC}/src/hal/spi_hal_iram.c"
          "${ORIG_SOC}/src/hal/spi_slave_hal.c"
          "${ORIG_SOC}/src/hal/spi_slave_hal_iram.c"
          "${ORIG_SOC}/src/hal/touch_sensor_hal.c"
          "${ORIG_SOC}/src/soc_include_legacy_warn.c"
          "${ORIG_SOC}/src/hal/pcnt_hal.c"
          "${ORIG_SOC}/src/hal/i2s_hal.c"
          "${ORIG_SOC}/src/hal/sigmadelta_hal.c"
          "${ORIG_SOC}/src/hal/timer_hal.c"
          "${ORIG_SOC}/src/hal/ledc_hal.c"
          "${ORIG_SOC}/src/hal/ledc_hal_iram.c"
          "${ORIG_SOC}/src/hal/i2c_hal.c"
          "${ORIG_SOC}/src/hal/i2c_hal_iram.c"
          "${ORIG_SOC}/src/hal/gpio_hal.c"
          "${ORIG_SOC}/src/hal/uart_hal.c"
          "${ORIG_SOC}/src/hal/uart_hal_iram.c"
          "${ORIG_SOC}/src/hal/spi_flash_hal.c"
          "${ORIG_SOC}/src/hal/spi_flash_hal_iram.c"
          "${ORIG_SOC}/src/hal/mpu_hal.c"
          "${ORIG_SOC}/src/hal/soc_hal.c"
          "${ORIG_SOC}/src/hal/wdt_hal_iram.c"
          "${ORIG_SOC}/src/compare_set.c"
          "${ORIG_SOC}/src/hal/twai_hal.c"
          "${ORIG_SOC}/src/hal/twai_hal_iram.c")

if(NOT BOOTLOADER_BUILD)
    list(APPEND srcs "${ORIG_SOC}/src/regi2c_ctrl.c")
endif()

set(priv_requires ${target})
if(${target} STREQUAL "esp32")
     # For RTC_EXT_CRYST_ADDIT_CURRENT to get chip revision in rtc_clk.c
     list(APPEND priv_requires efuse)
endif()

idf_component_register(SRCS ${srcs}
                       PRIV_REQUIRES ${priv_requires}
                       LDFRAGMENTS "${ORIG_SOC}/linker.lf")

if(CONFIG_IDF_TARGET_ESP32)
    target_sources(${COMPONENT_LIB} PRIVATE "${ORIG_SOC}/src/hal/mcpwm_hal.c"
                                            "${ORIG_SOC}/src/hal/sdio_slave_hal.c")
elseif(CONFIG_IDF_TARGET_ESP32S2)
    target_sources(${COMPONENT_LIB} PRIVATE "${ORIG_SOC}/src/hal/spi_flash_hal_gpspi.c")
endif()

# Since there can be chip-specific HAL headers which can include the common
# HAL header via include_next, process the build scripts here first so that
# include directories appear first in the compile command.
add_subdirectory(src/${target})
target_include_directories(${COMPONENT_LIB} PUBLIC "${ORIG_SOC}/include")

add_subdirectory(soc)
target_link_libraries(${COMPONENT_LIB} PUBLIC "soc_${target}")

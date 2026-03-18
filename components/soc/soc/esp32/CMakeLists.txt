set(ORIG_SOC_SOC_ESP32 $ENV{IDF_PATH}/components/soc/soc/esp32)

add_library(soc_esp32 STATIC
    "${ORIG_SOC_SOC_ESP32}/adc_periph.c"
    "${ORIG_SOC_SOC_ESP32}/dac_periph.c"
    "${ORIG_SOC_SOC_ESP32}/gpio_periph.c"
    "${ORIG_SOC_SOC_ESP32}/rtc_io_periph.c"
    "${ORIG_SOC_SOC_ESP32}/rtc_periph.c"
    "${ORIG_SOC_SOC_ESP32}/sdio_slave_periph.c"
    "${ORIG_SOC_SOC_ESP32}/sdmmc_periph.c"
    "${ORIG_SOC_SOC_ESP32}/interrupts.c"
    "${ORIG_SOC_SOC_ESP32}/spi_periph.c"
    "${ORIG_SOC_SOC_ESP32}/ledc_periph.c"
    "${ORIG_SOC_SOC_ESP32}/i2s_periph.c"
    "${ORIG_SOC_SOC_ESP32}/i2c_periph.c"
    "${ORIG_SOC_SOC_ESP32}/uart_periph.c"
    "${ORIG_SOC_SOC_ESP32}/touch_sensor_periph.c")

target_include_directories(soc_esp32 PUBLIC ${ORIG_SOC_SOC_ESP32}/include ${ORIG_SOC_SOC_ESP32}/../include ${ORIG_SOC_SOC_ESP32}/private_include)
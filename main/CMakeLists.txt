idf_component_register(SRCS ruuvidongle_main.c time.c mqtt.c gpio.c leds.c uart.c http.c settings.c
					REQUIRES mqtt lwip esp_http_client json esp32-wifi-manager
					INCLUDE_DIRS "includes"
)

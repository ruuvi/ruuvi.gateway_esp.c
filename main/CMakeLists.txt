idf_component_register(
    SRCS
        adv_decode_0x05.c
        adv_decode_0x06.c
        adv_decode_0xe0.c
        adv_decode_0xe1.c
        adv_decode_0xf0.c
        adv_decode.h
        adv_mqtt.c
        adv_mqtt.h
        adv_mqtt_cfg_cache.c
        adv_mqtt_cfg_cache.h
        adv_mqtt_events.c
        adv_mqtt_events.h
        adv_mqtt_internal.h
        adv_mqtt_signals.c
        adv_mqtt_signals.h
        adv_mqtt_task.c
        adv_mqtt_task.h
        adv_mqtt_timers.c
        adv_mqtt_timers.h
        adv_post.c
        adv_post.h
        adv_post_async_comm.c
        adv_post_async_comm.h
        adv_post_cfg_cache.c
        adv_post_cfg_cache.h
        adv_post_events.c
        adv_post_events.h
        adv_post_green_led.c
        adv_post_green_led.h
        adv_post_nrf52.c
        adv_post_nrf52.h
        adv_post_internal.h
        adv_post_signals.c
        adv_post_signals.h
        adv_post_statistics.c
        adv_post_statistics.h
        adv_post_task.c
        adv_post_task.h
        adv_post_timers.c
        adv_post_timers.h
        adv_table.c
        adv_table.h
        bin2hex.c
        bin2hex.h
        cjson_wrap.c
        cjson_wrap.h
        esp_ota_ops_patched.c
        esp_ota_ops_patched.h
        esp_tls_err.c
        esp_tls_err.h
        ethernet.c
        ethernet.h
        event_mgr.c
        event_mgr.h
        flashfatfs.c
        flashfatfs.h
        freertos_task_stack_size.c
        freertos_task_stack_size.h
        fw_update.c
        fw_update.h
        fw_ver.c
        fw_ver.h
        gpio.c
        gpio.h
        gpio_switch_ctrl.c
        gpio_switch_ctrl.h
        gw_cfg.c
        gw_cfg.h
        gw_cfg_blob.c
        gw_cfg_blob.h
        gw_cfg_cmp.c
        gw_cfg_json_parse.c
        gw_cfg_json_parse.h
        gw_cfg_json_parse_auto_update.c
        gw_cfg_json_parse_auto_update.h
        gw_cfg_json_parse_device_info.c
        gw_cfg_json_parse_device_info.h
        gw_cfg_json_parse_eth.c
        gw_cfg_json_parse_eth.h
        gw_cfg_json_parse_filter.c
        gw_cfg_json_parse_filter.h
        gw_cfg_json_parse_fw_update.c
        gw_cfg_json_parse_fw_update.h
        gw_cfg_json_parse_http.c
        gw_cfg_json_parse_http.h
        gw_cfg_json_parse_http_stat.c
        gw_cfg_json_parse_http_stat.h
        gw_cfg_json_parse_internal.c
        gw_cfg_json_parse_internal.h
        gw_cfg_json_parse_lan_auth.c
        gw_cfg_json_parse_lan_auth.h
        gw_cfg_json_parse_mqtt.c
        gw_cfg_json_parse_mqtt.h
        gw_cfg_json_parse_ntp.c
        gw_cfg_json_parse_ntp.h
        gw_cfg_json_parse_remote.c
        gw_cfg_json_parse_remote.h
        gw_cfg_json_parse_scan.c
        gw_cfg_json_parse_scan.h
        gw_cfg_json_parse_scan_filter.c
        gw_cfg_json_parse_scan_filter.h
        gw_cfg_json_parse_wifi.c
        gw_cfg_json_parse_wifi.h
        gw_cfg_json_generate.c
        gw_cfg_json_generate.h
        gw_cfg_log.c
        gw_cfg_log.h
        gw_cfg_ruuvi_json.c
        gw_cfg_ruuvi_json.h
        gw_cfg_default.c
        gw_cfg_default.h
        gw_cfg_default_json.c
        gw_cfg_default_json.h
        gw_cfg_storage.c
        gw_cfg_storage.h
        gw_cfg_storage_files.c
        gw_status.c
        gw_status.h
        gw_mac.c
        gw_mac.h
        hmac_sha256.c
        hmac_sha256.h
        http.c
        http.h
        http_check_mqtt.c
        http_json.c
        http_json.h
        http_post_advs.c
        http_post_stat.c
        json_ruuvi.c
        json_ruuvi.h
        http_download.c
        http_download.h
        http_server_cb.c
        http_server_cb.h
        http_server_cb_on_get.c
        http_server_cb_on_post.c
        leds_blinking.c
        leds_blinking.h
        leds_ctrl_defs.h
        leds_ctrl.c
        leds_ctrl.h
        leds_ctrl2.c
        leds_ctrl2.h
        leds.c
        leds.h
        main_loop.c
        metrics.c
        metrics.h
        mqtt.c
        mqtt.h
        mqtt_json.c
        mqtt_json.h
        network_subsystem.c
        network_subsystem.h
        network_timeout.c
        network_timeout.h
        nrf52_fw_ver.c
        nrf52_fw_ver.h
        nrf52fw.c
        nrf52fw.h
        nrf52swd.c
        nrf52swd.h
        partition_table.c
        partition_table.h
        reset_info.c
        reset_info.h
        reset_reason.c
        reset_reason.h
        reset_task.c
        reset_task.h
        runtime_stat.c
        runtime_stat.h
        ruuvi_device_id.c
        ruuvi_device_id.h
        ruuvi_gateway_main.c
        ruuvi_gateway.h
        ruuvi_nvs.c
        ruuvi_nvs.h
        settings.c
        time_str.c
        time_str.h
        time_task.c
        time_task.h
        url_encode.c
        url_encode.h
        validate_url.c
        validate_url.h
    PRIV_REQUIRES
        app_update
        mdns
        mqtt
        lwip
        esp_http_client
        json
        fatfs
        nvs_flash
        esp32-wifi-manager
        ruuvi.boards.c
        ruuvi.comm_tester.c
        ruuvi.endpoints.c
        libSWD-esp32
        ruuvi.esp_wrappers.c
        ruuvi.json_stream_gen.c
)

target_compile_options(${COMPONENT_LIB} PRIVATE
        -Wstack-usage=180
        -fstack-usage
)

# panic_print_char is used in the same module where it is implemented,
# so, it's impossible to redefine it with just "-Wl,--wrap=" option.
# Fortunately, there is a workaround described here:
# https://stackoverflow.com/questions/13961774/gnu-gcc-ld-wrapping-a-call-to-symbol-with-caller-and-callee-defined-in-the-sam

target_link_options(${COMPONENT_LIB} INTERFACE
        "-Wl,--wrap=esp_restart"
        "-Wl,--wrap,esp_panic_handler"
        "-Wl,--defsym,panic_print_char=panic_print_char_override"
        "-Wl,--wrap,panic_restart"
        "-Wl,--wrap,esp_err_to_name_r"
)

idf_build_get_property(target IDF_TARGET)

set(ESP_COMMON $ENV{IDF_PATH}/components/esp_common)

if(BOOTLOADER_BUILD)
    # For bootloader, all we need from esp_common is headers
    idf_component_register(INCLUDE_DIRS ${ESP_COMMON}/include)
    set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-Wl,--gc-sections")
else()
    # Regular app build
    set(srcs "${ESP_COMMON}/src/brownout.c"
             "${ESP_COMMON}/src/esp_err.c"
             "${ESP_COMMON}/src/dbg_stubs.c"
             "${ESP_COMMON}/src/esp_err_to_name.c"
             "${ESP_COMMON}/src/freertos_hooks.c"
             "${ESP_COMMON}/src/mac_addr.c"
             "${ESP_COMMON}/src/pm_locks.c"
             "${ESP_COMMON}/src/stack_check.c"
             "${ESP_COMMON}/src/task_wdt.c"
             "${ESP_COMMON}/src/int_wdt.c")

    # Note: esp_ipc added as a public requirement to keep compatibility as to be located here.
    idf_component_register(SRCS "${srcs}"
                           INCLUDE_DIRS ${ESP_COMMON}/include
                           REQUIRES ${target} espcoredump esp_timer esp_ipc
                           PRIV_REQUIRES soc
                           LDFRAGMENTS "${ESP_COMMON}/linker.lf")

    set_source_files_properties(
        "${ESP_COMMON}/src/stack_check.c"
        PROPERTIES COMPILE_FLAGS
        -fno-stack-protector)
    set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY LINK_LIBRARIES "-Wl,--gc-sections")
    set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-Wl,--gc-sections")
    set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY LINK_INTERFACE_MULTIPLICITY 4)

    # List of components needed for the error codes list
    set(optional_reqs ulp
                      efuse
                      esp_http_client
                      esp_http_server
                      bootloader_support
                      nvs_flash
                      esp_wifi
                      app_update
                      lwip
                      spi_flash
                      wpa_supplicant
                      tcpip_adapter)

    idf_build_get_property(build_components BUILD_COMPONENTS)
    foreach(req ${optional_reqs})
        if(req IN_LIST build_components)
            idf_component_get_property(req_lib ${req} COMPONENT_LIB)
            target_link_libraries(${COMPONENT_LIB} PRIVATE ${req_lib})
        endif()
    endforeach()

endif()

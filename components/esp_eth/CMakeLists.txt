idf_build_get_property(components_to_build BUILD_COMPONENTS)

idf_build_get_property(idf_path IDF_PATH)
if(NOT idf_path)
    message(FATAL_ERROR "IDF_PATH is not set or could not be determined by idf_build_get_property(IDF_PATH)")
endif()
set(ORIG "${idf_path}/components/esp_eth")

set(srcs)
set(include)
set(priv_requires)

# If Ethernet disabled in Kconfig, this is a config-only component
if(CONFIG_ETH_ENABLED)
    set(srcs "${ORIG}/src/esp_eth.c"
            "${ORIG}/src/esp_eth_phy.c")
    set(include "include")
    set(priv_requires "driver" "log") # require "driver" for using some GPIO APIs

    if(NOT CMAKE_BUILD_EARLY_EXPANSION)
        # esp_netif related
        if(esp_netif IN_LIST components_to_build)
            list(APPEND srcs "${ORIG}/src/esp_eth_netif_glue.c")
            list(APPEND priv_requires "esp_netif")
        endif()
    endif()

    if(CONFIG_ETH_USE_ESP32_EMAC)
        list(APPEND srcs "src/esp_eth_mac_esp32.c"
                         "${ORIG}/src/esp_eth_phy_dp83848.c"
                         "${ORIG}/src/esp_eth_phy_ip101.c"
                         "${ORIG}/src/esp_eth_phy_lan8720.c"
                         "${ORIG}/src/esp_eth_phy_rtl8201.c")
    endif()

    if(CONFIG_ETH_SPI_ETHERNET_DM9051)
        list(APPEND srcs "${ORIG}/src/esp_eth_mac_dm9051.c"
                         "${ORIG}/src/esp_eth_phy_dm9051.c")
    endif()

    if(CONFIG_ETH_USE_OPENETH)
        list(APPEND srcs "${ORIG}/src/esp_eth_mac_openeth.c")
    endif()
endif()

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS ${include}
                       REQUIRES "esp_event" # For using "ESP_EVENT_DECLARE_BASE" in header file
                       PRIV_REQUIRES ${priv_requires})

# uses C11 atomic feature
set_source_files_properties(${ORIG}/src/esp_eth.c PROPERTIES COMPILE_FLAGS -std=gnu11)

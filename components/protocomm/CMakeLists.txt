set(ORIG $ENV{IDF_PATH}/components/protocomm)

set(include_dirs ${ORIG}/include/common
                ${ORIG}/include/security
                ${ORIG}/include/transports)
set(priv_include_dirs ${ORIG}/proto-c ${ORIG}/src/common)
set(srcs
    "${ORIG}/src/common/protocomm.c"
    "${ORIG}/src/security/security0.c"
    "src/security/security1.c"
    "${ORIG}/proto-c/constants.pb-c.c"
    "${ORIG}/proto-c/sec0.pb-c.c"
    "${ORIG}/proto-c/sec1.pb-c.c"
    "${ORIG}/proto-c/session.pb-c.c"
    "${ORIG}/src/transports/protocomm_console.c"
    "${ORIG}/src/transports/protocomm_httpd.c")

if(CONFIG_BT_ENABLED)
    if(CONFIG_BT_BLUEDROID_ENABLED)
        list(APPEND srcs
            "${ORIG}/src/simple_ble/simple_ble.c"
            "${ORIG}/src/transports/protocomm_ble.c")
        list(APPEND priv_include_dirs
                ${ORIG}/src/simple_ble)
    endif()
    if(CONFIG_BT_NIMBLE_ENABLED)
        list(APPEND srcs
            "${ORIG}/src/transports/protocomm_nimble.c")
    endif()
endif()

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "${include_dirs}"
                    PRIV_INCLUDE_DIRS "${priv_include_dirs}"
                    PRIV_REQUIRES protobuf-c mbedtls console esp_http_server
                    REQUIRES bt)

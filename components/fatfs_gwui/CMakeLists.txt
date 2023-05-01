idf_component_register(PRIV_REQUIRES partition_table)

set(RUUVI_GWUI_HTML ${CMAKE_SOURCE_DIR}/ruuvi.gwui.html)
set(FATFS_GWUI_TMP_DIR ${CMAKE_BINARY_DIR}/fatfs_gwui)
set(FATFS_GWUI_IMG ${CMAKE_BINARY_DIR}/fatfs_gwui.bin)
set(MKFATFS ${CMAKE_BINARY_DIR}/mkfatfs/mkfatfs)

ExternalProject_Add(fatfs_gwui_ext
        PREFIX fatfs_gwui_ext
        DEPENDS mkfatfs
        SOURCE_DIR ${RUUVI_GWUI_HTML}
        BUILD_BYPRODUCTS ${FATFS_GWUI_IMG}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            ${CMAKE_SOURCE_DIR}/scripts/build_gwui_fatfs.sh ${RUUVI_GWUI_HTML} ${FATFS_GWUI_TMP_DIR} ${FATFS_GWUI_IMG} ${FATFS_GWUI_SIZE} ${MKFATFS}
        INSTALL_COMMAND ""
        BUILD_ALWAYS 1
)

esptool_py_flash_target_image(flash fatfs_gwui_ext "${FATFS_GWUI_ADDR}" "${FATFS_GWUI_IMG}")

add_dependencies(fatfs_gwui_ext partition_table)

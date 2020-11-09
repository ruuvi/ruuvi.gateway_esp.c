idf_component_register(PRIV_REQUIRES partition_table)

set(FATFS_GWUI_TMP_DIR ${CMAKE_BINARY_DIR}/fatfs_gwui)
set(FATFS_GWUI_IMG ${CMAKE_BINARY_DIR}/fatfs_gwui.bin)
set(MKFATFS ${CMAKE_BINARY_DIR}/mkfatfs/mkfatfs)
set(RUUVI_GWUI_HTML ${CMAKE_SOURCE_DIR}/ruuvi.gwui.html/src)

ExternalProject_Add(fatfs_gwui_ext
        PREFIX fatfs_gwui_ext
        DEPENDS mkfatfs
        SOURCE_DIR ${RUUVI_GWUI_HTML}
        BUILD_BYPRODUCTS ${FATFS_GWUI_IMG}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            python3 ${CMAKE_SOURCE_DIR}/scripts/prep_gwui.py -i=${RUUVI_GWUI_HTML} -o=${FATFS_GWUI_TMP_DIR} &&
            ${MKFATFS} -c ${FATFS_GWUI_TMP_DIR} -s ${FATFS_GWUI_SIZE} ${FATFS_GWUI_IMG}
        INSTALL_COMMAND ""
        BUILD_ALWAYS 1
)

esptool_py_flash_project_args(fatfs_gwui_ext
        ${FATFS_GWUI_ADDR}
        ${FATFS_GWUI_IMG}
        FLASH_IN_PROJECT
)

esptool_py_custom_target(fatfs_gwui fatfs_gwui_ext "fatfs_gwui")
add_dependencies(fatfs_gwui_ext partition_table)

idf_component_register(PRIV_REQUIRES partition_table)

set(FATFS_CFG_TMP_DIR ${CMAKE_BINARY_DIR}/fatfs_cfg)
set(FATFS_CFG_IMG ${CMAKE_BINARY_DIR}/fatfs_cfg.bin)
set(MKFATFS ${CMAKE_BINARY_DIR}/mkfatfs/mkfatfs)
set(RUUVI_GW_CFG_DEFAULT ${CMAKE_SOURCE_DIR}/gw_cfg_default)

ExternalProject_Add(fatfs_cfg_ext
        PREFIX fatfs_cfg_ext
        DEPENDS mkfatfs
        SOURCE_DIR ${RUUVI_GW_CFG_DEFAULT}
        BUILD_BYPRODUCTS ${FATFS_CFG_IMG}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            cp -R ${RUUVI_GW_CFG_DEFAULT}/. ${FATFS_CFG_TMP_DIR}/ &&
            ${MKFATFS} -c ${FATFS_CFG_TMP_DIR} -s ${FATFS_CFG_SIZE} ${FATFS_CFG_IMG}
        INSTALL_COMMAND ""
        BUILD_ALWAYS 1
)

esptool_py_flash_target_image(flash fatfs_cfg_ext "${FATFS_CFG_ADDR}" "${FATFS_CFG_IMG}")

add_dependencies(fatfs_cfg_ext partition_table)

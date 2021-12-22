idf_component_register(PRIV_REQUIRES partition_table)

set(DOWNLOAD_FW_HEX gw_nrf_fw.hex)
set(FATFS_NRF52_TMP_DIR ${CMAKE_BINARY_DIR}/fatfs_nrf52)
set(FATFS_NRF32_IMG ${CMAKE_BINARY_DIR}/fatfs_nrf52.bin)
set(MKFATFS ${CMAKE_BINARY_DIR}/mkfatfs/mkfatfs)

ExternalProject_Add(fatfs_nrf52_ext
        PREFIX fatfs_nrf52_ext
        DEPENDS mkfatfs
        URL ${RUUVI_NRF52_FW_URL}
        DOWNLOAD_NAME ${DOWNLOAD_FW_HEX}
        BUILD_BYPRODUCTS ${FATFS_NRF32_IMG}
        DOWNLOAD_NO_EXTRACT 1
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            python3 ${CMAKE_SOURCE_DIR}/scripts/ihex_to_segments.py -i=../${DOWNLOAD_FW_HEX} -o=${FATFS_NRF52_TMP_DIR} -f=info.txt -v=${RUUVI_NRF52_FW_VERSION} &&
            ${MKFATFS} -c ${FATFS_NRF52_TMP_DIR} -s ${FATFS_NRF52_SIZE} ${FATFS_NRF32_IMG}
        INSTALL_COMMAND ""
        BUILD_ALWAYS 1
)

esptool_py_flash_target_image(flash fatfs_nrf52_ext "${FATFS_NRF52_ADDR}" "${FATFS_NRF32_IMG}")

add_dependencies(fatfs_nrf52_ext partition_table)

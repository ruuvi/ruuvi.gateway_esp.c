idf_component_register(PRIV_REQUIRES partition_table)

set(DOWNLOAD_FW_HEX gw_nrf_fw.hex)
set(FATFS_NRF52_TMP_DIR ${CMAKE_BINARY_DIR}/fatfs_nrf52)
set(FATFS_NRF52_IMG ${CMAKE_BINARY_DIR}/fatfs_nrf52.bin)
set(MKFATFS ${CMAKE_BINARY_DIR}/mkfatfs/mkfatfs)

if(DEFINED RUUVI_NRF52_FW_URL)
    # It's a URL - download it
    ExternalProject_Add(fatfs_nrf52_ext
            PREFIX fatfs_nrf52_ext
            DEPENDS mkfatfs
            URL ${RUUVI_NRF52_FW_URL}
            DOWNLOAD_NAME ${DOWNLOAD_FW_HEX}
            DOWNLOAD_NO_EXTRACT 1
            CONFIGURE_COMMAND ""
            BUILD_COMMAND
                python3 ${CMAKE_SOURCE_DIR}/scripts/ihex_to_segments.py -i=../${DOWNLOAD_FW_HEX} -o=${FATFS_NRF52_TMP_DIR} -f=info.txt -v=${RUUVI_NRF52_FW_VERSION} &&
                ${MKFATFS} -c ${FATFS_NRF52_TMP_DIR} -s ${FATFS_NRF52_SIZE} ${FATFS_NRF52_IMG}
            INSTALL_COMMAND ""
            BUILD_ALWAYS 1
            BUILD_BYPRODUCTS ${FATFS_NRF52_IMG}
    )
else()
    if(NOT DEFINED RUUVI_NRF52_FW_BASE_DIR)
        message(FATAL_ERROR "RUUVI_NRF52_FW_URL is not defined, but RUUVI_NRF52_FW_BASE_DIR is also not defined.")
    endif()
    if(NOT DEFINED RUUVI_NRF52_FW_TARGET)
        message(FATAL_ERROR "RUUVI_NRF52_FW_URL is not defined, but RUUVI_NRF52_FW_TARGET is also not defined.")
    endif()
    execute_process(
            COMMAND git describe --tags --exact-match
            WORKING_DIRECTORY ${RUUVI_NRF52_FW_BASE_DIR}
            OUTPUT_VARIABLE VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
    )

    if(NOT VERSION)
        execute_process(
                COMMAND git rev-parse --short HEAD
                WORKING_DIRECTORY ${RUUVI_NRF52_FW_BASE_DIR}
                OUTPUT_VARIABLE VERSION
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()
    set(RUUVI_NRF52_FW_PATH "${RUUVI_NRF52_FW_BASE_DIR}/targets/${RUUVI_NRF52_FW_TARGET}/${RUUVI_NRF52_FW_TARGET}_armgcc_ruuvigw_release_${VERSION}_full.hex")

    message(STATUS "Firmware will be copied from local file: ${RUUVI_NRF52_FW_PATH}")

    # Create a custom target that runs every time CMake is called
    add_custom_target(fatfs_nrf52_build_fw ALL
            COMMAND bash -c "echo Building nRF52 firmware..."
            COMMAND make -C ${RUUVI_NRF52_FW_BASE_DIR} ${RUUVI_NRF52_FW_TARGET}
            COMMAND ${CMAKE_COMMAND} -E copy ${RUUVI_NRF52_FW_PATH} ${CMAKE_CURRENT_BINARY_DIR}/fatfs_nrf52_ext/${DOWNLOAD_FW_HEX}
            WORKING_DIRECTORY ${RUUVI_NRF52_FW_BASE_DIR}
    )

    # Create directory if it doesn't exist
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fatfs_nrf52_ext)

    ExternalProject_Add(fatfs_nrf52_ext
            PREFIX fatfs_nrf52_ext
            DEPENDS mkfatfs fatfs_nrf52_build_fw
            DOWNLOAD_COMMAND ""
            CONFIGURE_COMMAND ""
            BUILD_COMMAND
                python3 ${CMAKE_SOURCE_DIR}/scripts/ihex_to_segments.py -i=${CMAKE_CURRENT_BINARY_DIR}/fatfs_nrf52_ext/${DOWNLOAD_FW_HEX} -o=${FATFS_NRF52_TMP_DIR} -f=info.txt -v=${RUUVI_NRF52_FW_VERSION} &&
                ${MKFATFS} -c ${FATFS_NRF52_TMP_DIR} -s ${FATFS_NRF52_SIZE} ${FATFS_NRF52_IMG}
            INSTALL_COMMAND ""
            BUILD_ALWAYS 1
            BUILD_BYPRODUCTS ${FATFS_NRF52_IMG}
    )
endif()

add_dependencies(fatfs_nrf52_ext partition_table)

esptool_py_flash_target_image(flash fatfs_nrf52_ext "${FATFS_NRF52_ADDR}" "${FATFS_NRF52_IMG}")


idf_component_register(PRIV_REQUIRES partition_table)

set(DOWNLOAD_FW_HEX gw_nrf_fw.hex)

set(FATFS_NRF52_TMP_DIR ${CMAKE_BINARY_DIR}/fatfs_nrf52)

if(DEFINED RUUVI_NRF52_FW_URL)
    # It's a URL - download it
    ExternalProject_Add(fatfs_nrf52_ext
            PREFIX fatfs_nrf52_ext
            URL ${RUUVI_NRF52_FW_URL}
            DOWNLOAD_NAME ${DOWNLOAD_FW_HEX}
            DOWNLOAD_NO_EXTRACT 1
            CONFIGURE_COMMAND ""
            BUILD_COMMAND
                python3 ${CMAKE_SOURCE_DIR}/scripts/ihex_to_segments.py -i=../${DOWNLOAD_FW_HEX} -o=${FATFS_NRF52_TMP_DIR} -f=info.txt -v=${RUUVI_NRF52_FW_VERSION} &&
                ${CMAKE_SOURCE_DIR}/scripts/build_nrf52_fatfs.sh ${FATFS_NRF52_TMP_DIR} ${FATFS_NRF52_IMG} ${FATFS_NRF52_SIZE}
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
            DEPENDS fatfs_nrf52_build_fw
            DOWNLOAD_COMMAND ""
            CONFIGURE_COMMAND ""
            BUILD_COMMAND
                python3 ${CMAKE_SOURCE_DIR}/scripts/ihex_to_segments.py -i=${CMAKE_CURRENT_BINARY_DIR}/fatfs_nrf52_ext/${DOWNLOAD_FW_HEX} -o=${FATFS_NRF52_TMP_DIR} -f=info.txt -v=${RUUVI_NRF52_FW_VERSION} &&
                ${CMAKE_SOURCE_DIR}/scripts/build_nrf52_fatfs.sh ${FATFS_NRF52_TMP_DIR} ${FATFS_NRF52_IMG} ${FATFS_NRF52_SIZE}
            INSTALL_COMMAND ""
            BUILD_ALWAYS 1
            BUILD_BYPRODUCTS ${FATFS_NRF52_IMG}
    )
endif()

add_dependencies(fatfs_nrf52_ext partition_table)

esptool_py_flash_target_image(flash fatfs_nrf52_ext "${FATFS_NRF52_ADDR}" "${FATFS_NRF52_IMG}")

idf_build_get_property(build_dir BUILD_DIR)

if(CONFIG_SECURE_SIGNED_APPS_ECDSA_SCHEME)
    set(secure_boot_version "1")
elseif(CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME)
    set(secure_boot_version "2")
endif()

if(NOT BOOTLOADER_BUILD AND CONFIG_SECURE_SIGNED_APPS AND CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES)
    add_custom_command(
            OUTPUT "${build_dir}/.signed_nrf52fw_timestamp" "${NRF52_SIGNED}"
            COMMAND ${ESPSECUREPY} sign_data --version ${secure_boot_version} --keyfile ${secure_boot_signing_key}
            -o "${NRF52_SIGNED}" "${NRF52_UNSIGNED}"
            COMMAND ${CMAKE_COMMAND} -E echo "Generated signed binary image ${NRF52_SIGNED} from ${NRF52_UNSIGNED}"
            COMMAND ${CMAKE_COMMAND} -E md5sum "${NRF52_SIGNED}" > "${build_dir}/.signed_nrf52fw_timestamp"
            DEPENDS fatfs_nrf52_ext ${FATFS_NRF52_IMG}
            VERBATIM
            COMMENT "Signing nRF52 FATFS image"
    )
    add_custom_target(gen_signed_nrf52fw_binary ALL DEPENDS "${build_dir}/.signed_nrf52fw_timestamp")
    add_dependencies(gen_signed_nrf52fw_binary fatfs_nrf52_ext)


    idf_build_get_property(python PYTHON)
    # Extract the last 4096 bytes from the signed image into a separate file
    add_custom_command(
            OUTPUT "${NRF52_SIGNATURE}"
            COMMAND ${python} -c "import sys; f=open(sys.argv[1],'rb'); f.seek(-4096,2); open(sys.argv[2],'wb').write(f.read())" "${NRF52_SIGNED}" "${NRF52_SIGNATURE}"
            DEPENDS "${NRF52_SIGNED}"
            VERBATIM
            COMMENT "Extracting 4096-byte signature from ${NRF52_SIGNED} to ${NRF52_SIGNATURE}"
    )

    # Make it part of the default build, and ensure it runs after signing
    add_custom_target(gen_nrf52_signature ALL DEPENDS "${NRF52_SIGNATURE}")
    add_dependencies(gen_nrf52_signature gen_signed_nrf52fw_binary)
endif()

# Extract the last 3*4096 bytes from the unsigned image into a separate file
add_custom_command(
        OUTPUT "${NRF52_FATFS_WL_AREA}"
        COMMAND ${python} -c "import sys; f=open(sys.argv[1],'rb'); f.seek(-3*4096,2); open(sys.argv[2],'wb').write(f.read())" "${NRF52_UNSIGNED}" "${NRF52_FATFS_WL_AREA}"
        DEPENDS "${NRF52_UNSIGNED}"
        VERBATIM
        COMMENT "Extracting 3x4096-byte area used for wear-leveling on FAT-FS from ${NRF52_UNSIGNED} to ${NRF52_FATFS_WL_AREA}"
)
add_custom_target(gen_nrf52_fatfs_wl_area ALL DEPENDS "${NRF52_FATFS_WL_AREA}")
add_dependencies(gen_nrf52_fatfs_wl_area fatfs_nrf52_ext)

idf_component_register(PRIV_REQUIRES partition_table)

set(RUUVI_GWUI_HTML ${CMAKE_SOURCE_DIR}/ruuvi.gwui.html)
set(FATFS_GWUI_TMP_DIR ${CMAKE_BINARY_DIR}/fatfs_gwui)

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

idf_build_get_property(build_dir BUILD_DIR)

if(CONFIG_SECURE_SIGNED_APPS_ECDSA_SCHEME)
    set(secure_boot_version "1")
elseif(CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME)
    set(secure_boot_version "2")
endif()

if(NOT BOOTLOADER_BUILD AND CONFIG_SECURE_SIGNED_APPS AND CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES)
    add_custom_command(
            OUTPUT "${build_dir}/.signed_gwui_timestamp" "${GWUI_SIGNED}"
            COMMAND ${ESPSECUREPY} sign_data --version ${secure_boot_version} --keyfile ${secure_boot_signing_key}
            -o "${GWUI_SIGNED}" "${GWUI_UNSIGNED}"
            COMMAND ${CMAKE_COMMAND} -E echo "Generated signed binary image ${GWUI_SIGNED} from ${GWUI_UNSIGNED}"
            COMMAND ${CMAKE_COMMAND} -E md5sum "${GWUI_SIGNED}" > "${build_dir}/.signed_gwui_timestamp"
            DEPENDS fatfs_gwui_ext
            VERBATIM
            COMMENT "Signing GWUI FATFS image"
    )
    add_custom_target(gen_signed_gwui_binary ALL DEPENDS "${build_dir}/.signed_gwui_timestamp")
    add_dependencies(gen_signed_gwui_binary fatfs_gwui_ext)

    idf_build_get_property(python PYTHON)
    # Extract the last 4096 bytes from the signed image into a separate file
    add_custom_command(
            OUTPUT "${GWUI_SIGNATURE}"
            COMMAND ${python} -c "import sys; f=open(sys.argv[1],'rb'); f.seek(-4096,2); open(sys.argv[2],'wb').write(f.read())" "${GWUI_SIGNED}" "${GWUI_SIGNATURE}"
            DEPENDS "${GWUI_SIGNED}"
            VERBATIM
            COMMENT "Extracting 4096-byte signature from ${GWUI_SIGNED} to ${GWUI_SIGNATURE}"
    )

    # Make it part of the default build, and ensure it runs after signing
    add_custom_target(gen_gwui_signature ALL DEPENDS "${GWUI_SIGNATURE}")
    add_dependencies(gen_gwui_signature gen_signed_gwui_binary)
endif()

if(NOT BOOTLOADER_BUILD AND CONFIG_SECURE_SIGNED_APPS AND CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES)
    if (NOT DEFINED GWUI_SIGNATURE_ADDR)
        message(FATAL_ERROR "GWUI_SIGNATURE_ADDR is not defined")
    endif()
    esptool_py_flash_target_image(flash gen_gwui_signature ${GWUI_SIGNATURE_ADDR} "${GWUI_SIGNATURE}")
endif()

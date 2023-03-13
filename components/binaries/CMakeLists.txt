idf_component_register()

set(TARGET_DIR ${CMAKE_BINARY_DIR}/binaries_v1.9.2)

ExternalProject_Add(binaries_prj
        PREFIX binaries_prj
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
        BUILD_BYPRODUCTS ${TARGET_DIR}/bootloader.bin ${TARGET_DIR}/partition-table.bin
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            rm -rf ${TARGET_DIR} &&
            mkdir ${TARGET_DIR} &&
            cp ${CMAKE_CURRENT_SOURCE_DIR}/bootloader.bin ${TARGET_DIR} &&
            cp ${CMAKE_CURRENT_SOURCE_DIR}/partition-table.bin ${TARGET_DIR}
        INSTALL_COMMAND ""
        BUILD_ALWAYS 1
)

idf_component_register(PRIV_REQUIRES partition_table)

set(GW_CFG_DEF_IMG ${CMAKE_BINARY_DIR}/gw_cfg_def.bin)
#set(NVS_PARTITION_GEN $ENV{IDF_PATH}/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py)
set(NVS_PARTITION_GEN ${CMAKE_SOURCE_DIR}/scripts/nvs_partition_gen.py)
set(RUUVI_GW_CFG_DEFAULT ${CMAKE_SOURCE_DIR}/gw_cfg_default)

ExternalProject_Add(gw_cfg_def_prj
        PREFIX gw_cfg_def_prj
        SOURCE_DIR ${RUUVI_GW_CFG_DEFAULT}
        BUILD_BYPRODUCTS ${GW_CFG_DEF_IMG}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            cp -R ${RUUVI_GW_CFG_DEFAULT}/. ./ &&
            python ${NVS_PARTITION_GEN} generate ${RUUVI_GW_CFG_DEFAULT}/gw_cfg_def_partition.csv ${GW_CFG_DEF_IMG} ${GW_CFG_DEF_SIZE}
        INSTALL_COMMAND ""
        BUILD_ALWAYS 1
)

esptool_py_flash_target_image(flash gw_cfg_def_prj "${GW_CFG_DEF_ADDR}" "${GW_CFG_DEF_IMG}")

add_dependencies(gw_cfg_def_prj partition_table)

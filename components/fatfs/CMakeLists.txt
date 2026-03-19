idf_build_get_property(idf_path IDF_PATH)
if(NOT idf_path)
    message(FATAL_ERROR "IDF_PATH is not set or could not be determined by idf_build_get_property(IDF_PATH)")
endif()
set(ORIG "${idf_path}/components/fatfs")

set(srcs "${ORIG}/diskio/diskio.c"
         "diskio/diskio_rawflash.c"
         "${ORIG}/diskio/diskio_sdmmc.c"
         "${ORIG}/diskio/diskio_wl.c"
         "${ORIG}/src/ff.c"
         "${ORIG}/port/freertos/ffsystem.c"
         "${ORIG}/src/ffunicode.c"
         "${ORIG}/vfs/vfs_fat.c"
         "${ORIG}/vfs/vfs_fat_sdmmc.c"
         "${ORIG}/vfs/vfs_fat_spiflash.c")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "${ORIG}/diskio" "${ORIG}/vfs" "${ORIG}/src"
                       REQUIRES wear_levelling sdmmc
                      )

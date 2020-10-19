cmake_minimum_required(VERSION 3.5)

project(mkfatfs)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

set(MKFATFS_SRC             ../ESP32_mkfatfs/components/mkfatfs/src)
set(MKFATFS_SRC_FATFS       ${MKFATFS_SRC}/fatfs)
set(IDF_MODIFIED_DIR        ${MKFATFS_SRC}/idf/modified)
set(IDF_ORIG_DIR            ${MKFATFS_SRC}/idf/orig)

include_directories(
        ${CMAKE_BINARY_DIR}/../config
        ${IDF_MODIFIED_DIR}
        ${IDF_MODIFIED_DIR}/driver/include
        ${IDF_MODIFIED_DIR}/fatfs/src
        ${IDF_MODIFIED_DIR}/freertos/include
        ${IDF_MODIFIED_DIR}/log/include
        ${IDF_MODIFIED_DIR}/newlib/include
        ${IDF_MODIFIED_DIR}/vfs/include
        ${IDF_MODIFIED_DIR}/vfs/include/sys
        ${IDF_ORIG_DIR}/driver/include
        ${IDF_ORIG_DIR}/driver/include/driver
        ${IDF_ORIG_DIR}/esp32/include
        ${IDF_ORIG_DIR}/fatfs/src
        ${IDF_ORIG_DIR}/sdmmc/include
        ${IDF_ORIG_DIR}/spi_flash/include
        ${IDF_ORIG_DIR}/wear_levelling/include
        ${IDF_ORIG_DIR}/wear_levelling/private_include
        ${MKFATFS_SRC}/tclap
        ${MKFATFS_SRC}/fatfs
)

add_executable(mkfatfs
        ${CMAKE_BINARY_DIR}/../config/sdkconfig.h
        ${MKFATFS_SRC}/main.cpp
        ${MKFATFS_SRC_FATFS}/fatfs.c
        ${MKFATFS_SRC_FATFS}/ccsbcs.c
        ${MKFATFS_SRC_FATFS}/crc.cpp
        ${MKFATFS_SRC_FATFS}/FatPartition.cpp
        ${IDF_MODIFIED_DIR}/fatfs/src/ff.c
        ${IDF_MODIFIED_DIR}/fatfs/src/vfs_fat.c
        ${IDF_MODIFIED_DIR}/freertos/include/freertos/semphr.c
        ${IDF_MODIFIED_DIR}/newlib/include/sys/lock.c
        ${IDF_MODIFIED_DIR}/newlib/include/sys/idf_reent.c
        ${IDF_MODIFIED_DIR}/newlib/include/sys/errno.c
        ${IDF_MODIFIED_DIR}/spi_flash/partition.c
        ${IDF_MODIFIED_DIR}/vfs/vfs.c
        ${IDF_MODIFIED_DIR}/wear_levelling/wear_levelling.cpp
        ${IDF_ORIG_DIR}/fatfs/src/diskio.c
        ${IDF_ORIG_DIR}/fatfs/src/diskio_spiflash.c
        ${IDF_ORIG_DIR}/fatfs/src/option/syscall.c
        ${IDF_ORIG_DIR}/wear_levelling/crc32.cpp
        ${IDF_ORIG_DIR}/wear_levelling/WL_Flash.cpp
        ${IDF_ORIG_DIR}/wear_levelling/WL_Ext_Perf.cpp
        ${IDF_ORIG_DIR}/wear_levelling/WL_Ext_Safe.cpp
)

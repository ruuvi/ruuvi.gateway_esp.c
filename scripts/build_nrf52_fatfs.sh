#!/bin/bash

set -x -e

SCRIPT=$(realpath "$0")
SCRIPT_PATH=$(dirname "$SCRIPT")

CMAKE_BINARY_DIR=$SCRIPT_PATH/../build

if [ -z "$1" ]
  then
    FATFS_NRF52_TMP_DIR=$CMAKE_BINARY_DIR/fatfs_nrf52
  else
    FATFS_NRF52_TMP_DIR=$1
fi

if [ -z "$2" ]
  then
    FATFS_NRF52_IMG=$CMAKE_BINARY_DIR/fatfs_nrf52.bin
  else
    FATFS_NRF52_IMG=$2
fi

if [ -z "$3" ]
  then
    FATFS_NRF52_SIZE=0x40000
  else
    FATFS_NRF52_SIZE=$3
fi

truncate -s $(($FATFS_NRF52_SIZE)) "$FATFS_NRF52_IMG"
export SOURCE_DATE_EPOCH=315532800  # 1980-01-01 00:00:00 UTC (to create reproducible builds)
mformat -i "$FATFS_NRF52_IMG" -v NRF52 -N 0 ::
mcopy -i "$FATFS_NRF52_IMG" -v -s $FATFS_NRF52_TMP_DIR/* ::

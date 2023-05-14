#!/bin/bash

set -x -e

SCRIPT=$(realpath "$0")
SCRIPT_PATH=$(dirname "$SCRIPT")

CMAKE_BINARY_DIR=$SCRIPT_PATH/../build

if [ -z "$1" ]
  then
    RUUVI_GWUI_HTML=$SCRIPT_PATH/../ruuvi.gwui.html
  else
    RUUVI_GWUI_HTML=$1
fi

if [ -z "$2" ]
  then
    FATFS_GWUI_TMP_DIR=$CMAKE_BINARY_DIR/fatfs_gwui
  else
    FATFS_GWUI_TMP_DIR=$2
fi

if [ -z "$3" ]
  then
    FATFS_GWUI_IMG=$CMAKE_BINARY_DIR/fatfs_gwui.bin
  else
    FATFS_GWUI_IMG=$3
fi

if [ -z "$4" ]
  then
    FATFS_GWUI_SIZE=0xC0000
  else
    FATFS_GWUI_SIZE=$4
fi

if [ -z "$5" ]
  then
    MKFATFS=$CMAKE_BINARY_DIR/mkfatfs/mkfatfs
  else
    MKFATFS=$5
fi

cd "$RUUVI_GWUI_HTML"
[ ! -d "$RUUVI_GWUI_HTML/node_modules" ] && npm install
npm run build-prod
npm run test
python3 "$SCRIPT_PATH/prep_gwui.py" -i="$RUUVI_GWUI_HTML/build" -o="$FATFS_GWUI_TMP_DIR"
"$MKFATFS" -c "$FATFS_GWUI_TMP_DIR" -s "$FATFS_GWUI_SIZE" "$FATFS_GWUI_IMG"

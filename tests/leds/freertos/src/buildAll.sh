#!/bin/bash
cd "$(dirname "$0")"

make -C FreeRTOS_Posix/Debug/ -f makefile $1 $2 $3


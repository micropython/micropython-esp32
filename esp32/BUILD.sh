#!/bin/bash

# ###############################################
# This is the clone of the MicroPython repository
# (https://github.com/micropython/micropython)
# with added ESP32 build
# ###############################################

# Usage:
# ./BUILD           - run the build, create MicroPython firmware
# ./BUILD clean     - clean the build
# ./BUILD deploy    - flash MicroPython firmware to ESP32
# ./BUILD erase     - erase the whole ESP32 Flash
# ./BUILD monitor   - run esp-idf terminal program

# ###########################################################################
# build MicroPython cross compiler which compiles .py scripts into .mpy files
# ###########################################################################
make -C ../mpy-cross

export HOST_PLATFORM=linux

# ########################################
# Add Xtensa toolchain path to system path
# ########################################
export PATH=<path_to>/xtensa-esp32-elf/bin:$PATH

# ###################
# Export esp-idf path
# ###################
export IDF_PATH=<path_to>/esp-idf

# ##################################
# Export Micropython build variables
# ##################################
export CROSS_COMPILE=xtensa-esp32-elf-
export ESPIDF=$IDF_PATH
export PORT=/dev/ttyUSB1
export FLASH_MODE=dio
export FLASH_SIZE=4MB
export BAUD=921600
export DEPLOY_ERASE_RESET=no_reset

# ##########################
# build MicroPython firmware
# ##########################
cd esp32

make "$@"

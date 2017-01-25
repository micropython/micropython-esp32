#!/bin/bash
############
# This script checks the ESP-IDF version against supported versions, and will download and checkout 
# the latest supported ESP-IDF. It does this locally and does not gloablly set environment variables
# because the point is to avoid messing with global ESP-IDF environments. If you want to do that, 
# just do it.

# We need ESPIDF for make to run at all - I should fix the Makefile for that


if [ ! $ESPIDF  ]
then
	export ESPIDF="./"
fi



cd ../
make -C mpy-cross
cd esp32

VER=`make idf-version` 
#echo $VER
IDF_VER=`sed s/.*hash://g <<< $VER` 
IDF_VER=`sed s/\s//g <<< $IDF_VER` 
#echo "VERSION REQUIRED: -$IDF_VER-"  
if [ ! -d esp-idf ]
then
	git clone --recursive https://github.com/espressif/esp-idf 
fi 
cd esp-idf  
GIT_VER=`git -C ./ describe `
#echo $GIT_VER  
if [ ! $GIT_VER == $IDF_VER ]
then
	git checkout  $IDF_VER 
fi

cd ../ 
IDF_PATH=./esp-idf 
ESPIDF=./esp-idf 

if [ $1 == "deploy" ]
then
	MAKEFLAGS="FLASH_MODE=dio "
fi
MAKEFLAGS="$MAKEFLAGS $1"


make $MAKEFLAGS



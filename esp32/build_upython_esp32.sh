#!/bin/bash


# We need ESPIDF for make to run at all
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
make deploy


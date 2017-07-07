#!/usr/bin/env bash

while true
do 
	echo -e '\033[44m'
       	clear
       	if [ -r $PORT ] 
	then
		echo -e '\033[41m'
		clear
	       	make
	       	deploy
		while [ -r $PORT ]
		do
		       echo -e '\033[42m'
		       clear
		       sleep 1
	       done
       fi
       sleep 1
done


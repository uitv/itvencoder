#!/bin/bash

x264info=`rpm -qa x264`

if [ ! -z "$x264info" ] ; then
	echo "Uninstall the RPM libx264."
	rpm -e x264 --nodeps
	if [ ! $? -eq 0 ] ; then
        	echo "Uninstall RPM libx264 FAILED!!!"
	        exit 1
	fi
fi


echo "Install libx264 ..."
cd ./x264
./install.sh
if [ ! $? -eq 0 ] ; then
        echo "Install libx264 FAILED!!!"
	exit 1
else
	echo "Install libx264 SUCCESSFUL"
fi
cd ..



echo "Install libgstx264enc ..."
cd ./libgstx264enc
./install.sh
if [ ! $? -eq 0 ] ; then
	echo "Install libgstx264enc FAILED!!!"
	exit 1
else
	echo "Install libgstx264enc SUCCESSFUL"
fi
cd ..


echo "Install mvideoscale ..."
cd ./mvideoscale
./install.sh
if [ ! $? -eq 0 ] ; then
        echo "Install mvideoscale FAILED!!!"
        exit 1
else
        echo "Install mvideoscale SUCCESSFUL"
fi
cd ..

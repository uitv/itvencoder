#!/bin/bash

echo "Uninstall libx264"
cd ./x264
./uninstall.sh
if [ ! $? -eq 0 ] ; then
        echo "Uninstall libx264 FAILED!!!"
else
        echo "Uninstall libx264 SUCCESSFUL"
fi
cd ..



echo "Uninstall libgstx264enc"
cd ./libgstx264enc
./uninstall.sh
if [ ! $? -eq 0 ] ; then
        echo "Uninstall libgstx264enc FAILED!!!"
else
        echo "Uninstall libgstx264enc SUCCESSFUL"
fi
cd ..


echo "Uninstall mvideoscale"
cd ./mvideoscale
./uninstall.sh
if [ ! $? -eq 0 ] ; then
        echo "Uninstall mvideoscale FAILED!!!"
else
        echo "Uninstall mvideoscale SUCCESSFUL"
fi
cd ..

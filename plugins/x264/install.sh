#!/bin/bash

#cp ./x264.h /usr/include
#cp ./x264_config.h /usr/include
#cp ./x264.pc /usr/lib64/pkgconfig
#cp ./libx264.so.129 /usr/lib64/
#ln -f -s /usr/lib64/libx264.so /usr/lib64/libx264.so.129
#chmod 755 /usr/lib64/libx264.so.129

install -b -p ./x264.h /usr/include
install -b -p ./x264_config.h /usr/include
install -b -p ./x264.pc /usr/lib64/pkgconfig
install -b -p -m 0755 ./libx264.so.129 /usr/lib64/
if [ -e /usr/lib64/libx264.so ] ; then
	mv -f /usr/lib64/libx264.so /usr/lib64/libx264.so~
fi
ln -f -s /usr/lib64/libx264.so.129 /usr/lib64/libx264.so

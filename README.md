# Introduction

iTVEncoder is a real time encoder using gstreamer.

# Install from source

## Prerequisites

gnome-common

autoconf

automake

libtool

gstreamer-devel

gstreamer-plugins-base-devel

## Compile and install

    ./autogen.sh
    ./configure (--help)
    make
    make install

## Run

    cd /etc/itvencoder
    cp itvencoder.conf.test itvencoder.conf
    sudo itvencoder

in browser open http://your.host:httpmgmtport/mgmt

[Management](itvencoder/docs/management.md)

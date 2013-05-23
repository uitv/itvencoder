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

## Configuration and Run

    cd /etc/itvencoder
    cp itvencoder.conf.test itvencoder.conf
    sudo itvencoder

You can customize iTVEncoder by edit configuration file, for detail information, reference [Configure](itvencoder/docs/configure.md).

## Management

You can management iTVEncoder throuh browser, open http://your.host:httpmgmtport/mgmt.

iTVEncoder support RESTful compatibale managment interface, for more information, reference [Management](itvencoder/docs/management.md).

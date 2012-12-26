This packet is including "libx264" and Gstreamer element "x264enc",
if you want to install(or uninstall) it.
Please followng the below steps.

1.Install
./install.sh

2.Uninstall
./uninstall.sh



*********************************************************************
NOTICE:
1.About the "libgstx264enc"
"libgstx264enc" DEPENDS on "libx264", so if you want to use the
libgestx264enc, you must install libx264 first.

Another hand. Because our system need to GOP aligned, and the old
libgstx264enc can not provide enough parameters and logical-process. So I modified
the source code.

2.About the libx264
Because the GOP aligned need some advance property, and the new-version can
provide these request. So I update the libx264.

If you wang to chack out the lastest source-code. I think you can use the command line 
"git clone git://git.videolan.org/x264.git".

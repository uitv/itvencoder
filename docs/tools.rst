相关工具介绍
************

在调试配置iTVEncoder的过程中，需要用到一些其他的工具软件，这些工具软件包括gst-launch, gst-inspect, tsreport, tsplay, ffprobe等。典型的应用比如源为ts over ip的时候，需要知道视频轨、音轨以及字幕轨的pid，这是因为iTVEncoder支持多音轨多字幕，因此需要知道对应音频或者字幕的pid。如下命令可以知道ts流的构成::

    gst-launch udpsrc uri=udp://239.100.191.8:1234 ! mpegtsdemux ! fakesink

确认已经开始运行::

    /GstPipeline:pipeline0/GstMpegTSDemux:mpegtsdemux0: pat-info = ((GValueArray*) 0xc2cee0)
    /GstPipeline:pipeline0/GstMpegTSDemux:mpegtsdemux0: pmt-info = ((MpegTsPmtInfo*) 0xc098f0)
    /GstPipeline:pipeline0/GstFakeSink:fakesink0.GstPad:sink: caps = audio/mpeg, mpegversion=(int)1
    /GstPipeline:pipeline0/GstFakeSink:fakesink0: last-message = "preroll   ******* "
    /GstPipeline:pipeline0/GstFakeSink:fakesink0: last-message = "event   ******* E (type: 102, GstEventNewsegment, update=(boolean)false, rate=(double)1, applied-rate=(double)1, format=(GstFormat)GST_FORMAT_TIME, start=(gint64)29213604944444, stop=(gint64)-1, position=(gint64)0;) 0x7f3f28016640"
    /GstPipeline:pipeline0/GstFakeSink:fakesink0: last-message = "event   ******* E (type: 118, taglist, language-code=(string)zh;) 0x7f3f28016680"
    /GstPipeline:pipeline0/GstFakeSink:fakesink0: last-message = "chain   ******* < (  168 bytes, timestamp: 8:06:53.695555555, duration: none, offset: -1, offset_end: -1, flags: 32) 0x7f3f28006040"
    
Ctrl + C 中断后显示::
    
    handling interrupt.
    Interrupt: Stopping pipeline ...
    Execution ended after 499997221 ns.
    Setting pipeline to PAUSED ...
    Setting pipeline to READY ...
    /GstPipeline:pipeline0/GstFakeSink:fakesink0.GstPad:sink: caps = NULL
    /GstPipeline:pipeline0/GstMpegTSDemux:mpegtsdemux0.GstPad:video_0532: caps = NULL
    /GstPipeline:pipeline0/GstMpegTSDemux:mpegtsdemux0.GstPad:audio_0533: caps = NULL

本章接下来的内容将对可能用到的linux环境下的工具做一个概要的介绍。Windows环境下可能用到的工具比如vlc，ffmpeg以及elecard系列工具等，这里暂时不做介绍。

gst-launch
==========

gst-launch用于创建并运行gstreamer pipeline。在CentOS下，gst-launch在gstreamer包中。配置iTVEncoder的时，需要先对某个element有一个直观的了解，这个时候就可以用gst-launch创建一个包含这个element的pipeline并运行，比如像了解udpsrc这个element的使用::

    gst-launch udpsrc uri=udp://239.100.191.8:1234 ! filesink location=xyz.ts

针对ip输入，如果想先看看推送到服务器上的某个流的播放情况，可以用gst-launch把这个流转发到自己的桌面计算机上，用诸如vlc等播放器进行播放::

    gst-launch-0.10 udpsrc uri=udp://239.100.191.8:1234 ! udpsink host=192.168.8.115 port=12345

在桌面计算机上用vlc播放地址::

    udp://@192.168.8.115:12345

gst-launch的更详细文档可以参阅gst-launch的手册页。

gst-inspect
===========

gst-inspect用于列出gstreamer插件或者element的相关信息。调试iTVEncoder的时候，查询要哪一个element实现想要的某项功能，比如查找能实现mpegtsmux的element::

    gst-inspect | grep mpegts
    
这个命令的结果如下::
    
    ffmpeg:  ffmux_mpegts: FFmpeg MPEG-2 transport stream format muxer (not recommended, use mpegtsmux instead)
    mpegtsmux:  mpegtsmux: MPEG Transport Stream Muxer
    mpegtsdemux:  tsdemux: MPEG transport stream demuxer
    mpegtsdemux:  tsparse: MPEG transport stream parser
    mpegdemux2:  mpegtsparse: MPEG transport stream parser
    mpegdemux2:  mpegtsdemux: The Fluendo MPEG Transport stream demuxer
    typefindfunctions: video/mpegts: ts, mts

再比如想了解某个element的具体信息，比如udpsrc::

    gst-inspect udpsrc

更多关于gst-inspect的信息，参考gst-inspect的手册页。

tsreport
========

tsreport是tstools工具集中的一个，用于列出给定ts流的相关信息，比如ts包的个数，ts流的构成，pcr相关信息，pts/dts相关信息等。tstools的主页是: http://tstools.berlios.de/。

tsplay
======

tsplay是tstools工具集中的一个，用于把ts流通过udp、tcp协议或者标准输出推送出去，可以循环推送一个文件。

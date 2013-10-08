概述
****

iTVEncoder是基于gstreamer的实时编码器。

与通常的编码器不同的是，可以通过编写gstreamer插件和修改iTVEncoder配置文件的方式对编码器进行扩展定制。

要对iTVEncoder进行定制，首先需要熟悉gstreamer架构，还要熟悉gstreamer工具的使用。关于gstramer的相关内容可以访问 http://gstreamer.freedesktop.org。

要对iTVEncoder进行扩展，则需要掌握gstreamer插件编程，根据需求编写相应的插件，并编写相应的iTVEncoder配置文件来实现iTVEncoder的功能扩展。

gstreamer
=========

Gstreamer是用于流媒体应用开发的框架，Gstreamer是基于plugins的，包括Gstreamer core和若干elements，一个plugin包含一个或者多个elements。基于Gstreamer的应用程序能够具备的处理能力依赖于系统中安装的不同种类功能的elements的数量。Gstreamer核心不具备处理具体的media的功能，但是element处理media时需要具备的特性很多是由Gstreamer的核心提供的。

Gstreamer core实现了gstreamer架构，给出了数据流如何处理，规定了媒体类型的协商，给出了编写应用的API。利用Gstreamer编写多媒体应用程序，就是利用elements构建一个pipeline。

element是pipeline的最小组成部分，element是一个对多媒体流进行处理的object，一个element对多媒体流进行的处理可能是读取文件、音频或者视频解码、音频或者视频编码、从硬件采集设备上采集数据、在硬件设备上播放多媒体、多个流的复用、多个流的解复用。

通过下图理解gstreamer框架及基于gstreamer的应用：

.. image:: _static/gstreamer-overview.png

一下分别介绍gstreamer中的基本概念，包括pipeline、bin、element、pads、caps等。

element
-------

elements的输入叫做sink pads，输出叫做source pads。应用程序通过pad把element连接起来构成pipeline，如下图所示，其中顺着流的方向为downstream，相反方向是upstream。

.. image:: _static/pipeline.png

iTVEncoder
==========

.. image:: _static/itvencoderarch.png

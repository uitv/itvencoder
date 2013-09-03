iTVEncoder安装部署
******************

安装操作系统和gstreamer
=======================

目前只在64位的CentOS 6.2上进行过验证。安装CentOS的时候选择Basic Server，这样安装完成后即安装了gstreamer-plugins-base、gstreamer-tools、gstreamer等，还需要安装如下插件::

    gstreamer-plugins-bad-free
    gstreamer-plugins-good

然后安装一个rpmforge源::

    sudo rpm -ivh http://pkgs.repoforge.org/rpmforge-release/rpmforge-release-0.5.2-2.el6.rf.x86_64.rpm

并安装如下三个gstreamer插件::

    gstreamer-ffmpeg
    gstreamer-plugins-bad
    gstreamer-plugins-ugly

iTVEncoder源码安装
==================

iTVEncoder已经随整个安装包一起安装，这里介绍如何从源代码安装和生成rpm包。

源码编译安装::

    ./autogen.sh
    ./configure (--help)
    make
    make install

生成rpm安装包::

    make dist
    cp itvencoder-version.tar.gz ~/rpmbuild/SOURCES/
    rpmbuild -bb itvencoder.spec

以上步骤是在CentOS 6.2操作系统环境下进行的，在其它系统上可能会稍有差别，生成的rpm包在目录~/rpmbuild/RPMS/x86_64/中。

iTVEncoder运行
==============

itvencoder运行时主要参数如下::

    -c, --config                      -c /full/path/to/itvencoder.conf: Specify a config file, full path is must.
    -m, --mediainfo                   -m media uri, extract media info of the uri.
    -d, --foreground                  Run in the foreground
    -s, --stop                        Stop itvencoder.
    -v, --version                     display version information and exit.

itvencoder采用了gstreamer的命令行选项::
    
    Help Options:
      -h, --help                        Show help options
      --help-all                        Show all help options
      --help-gst                        Show GStreamer Options
    
    GStreamer Options
      --gst-version                     Print the GStreamer version
      --gst-fatal-warnings              Make all warnings fatal
      --gst-debug-help                  Print available debug categories and exit
      --gst-debug-level=LEVEL           Default debug level from 1 (only error) to 5 (anything) or 0 for no output
      --gst-debug=LIST                  Comma-separated list of category_name:level pairs to set specific levels for the individual categories. Example: GST_AUTOPLUG:5,GST_ELEMENT_*:3
      --gst-debug-no-color              Disable colored debugging output
      --gst-debug-disable               Disable debugging
      --gst-plugin-spew                 Enable verbose plugin loading diagnostics
      --gst-plugin-path=PATHS           Colon-separated paths containing plugins
      --gst-plugin-load=PLUGINS         Comma-separated list of plugins to preload in addition to the list stored in environment variable GST_PLUGIN_PATH
      --gst-disable-segtrap             Disable trapping of segmentation faults during plugin loading
      --gst-disable-registry-update     Disable updating the registry
      --gst-disable-registry-fork       Disable spawning a helper process while scanning the registry
    
如果想要运行在3的log级别，在前台运行::

    itvencoder -d --gst-debug=3

如果想让itvencoder这个cagegory运行在3的log级别，在后台运行::

    itvencoder --gst-debug=itvencoder:3

itvencoder针对每一个channel有一个log文件，针对channel的log文件在/log/path/channel-name/itvencoder.log。

itvencoder内置了logrotate功能，默认log文件大小是2MBytes，rotate个数是100。

如果想把itvencoder用作获取媒体信息的工具::

    itvencoder -m udp://225.1.1.3:6002

返回的结果如下::

    <?xml version="1.0" encoding="utf-8"?>
    <TS>
    <PAT tsid="1" version="5" current_next="1">
    <PROGRAM number="1331" pid="257"/>
    </PAT>
    NIT is carried on PID 31 which isn't DVB compliant
    <PAT tsid="1" version="0" current_next="1">
    <PROGRAM number="0" pid="31"/>
    <PROGRAM number="1" pid="256"/>
    </PAT>
    <PMT program="1" version="0" current_next="1" pcrpid="4097">
    <DESC id="0x05" length="4" value="48444d56">
    <REGISTRATION_DESC identifier="HDMV"/>
    </DESC>
    <DESC id="0x88" length="4" value="0ffffcfc">
    <UNKNOWN_DESC />
    </DESC>
    <ES pid="0x1011" streamtype="0x1b" streamtype_txt="H.264/14496-10 video (MPEG-4/AVC)">
    <DESC id="0x28" length="4" value="4d4029bf">
    <AVC_VIDEO_DESC profile_idc="0x4d" constraint_set0_flag="0" constraint_set1_flag="1" constraint_set2_flag="0" AVC_compatible_flags="0x00" level_idc="0x29" AVC_still_present="1" AVC_24_hour_picture_flag="0"/>
    </DESC>
    </ES>
    <ES pid="0x1100" streamtype="0x0f" streamtype_txt="13818-7 Audio with ADTS transport syntax">
    </ES>
    </PMT>
    </TS>

通过-c可以指定配置文件，通过指定配置文件可以启动多个itvencoder，需要注意的是，配置文件需要绑定不同的服务端口。


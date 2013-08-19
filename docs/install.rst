iTVEncoder安装部署
******************

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

启动多个itvencoder
==================

由于gstreamer的pipeline在运行状态下关闭并重新启动存在如下问题，第一是有时候关闭成功需要较长时间，根据以往的log信息发现最长时间需要半个小时。
第二是有时候关闭并重新启动一个pipeline存在内存泄露。

itvencoder运行过程中，由于解码出来的音视频数据严重不同步，解码出错，编码出现问题等原因，需要重新启动pipeline，因此重启是不可避免的。

鉴于上面的原因，可以让一个通道对应一个itvencoder进程，也就是只为itvencoder配置一个通道，重启的时候不是采用streamer的相关功能，而是重新启动进程。

itvencoder提供了-c选项，可以在运行的时候指定不同的配置文件来运行多个itvencoder，这样就运行了多个频道，需要注意的是，配置文件的server部分不能冲突。


当前最新版本已经实现了每个通道启动一个进程，因此应经可以不必通过启动多个itvencoder的方式来绕过重启通道的问题。

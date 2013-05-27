iTVEncoder安装部署
******************

iTVEncoder已经随整个安装包一起安装，这里不再单独说明。

iTVEncoder运行
==============

itvencoder运行时主要参数如下::

    -c, --config                      -c /full/path/to/itvencoder.conf: Specify a config file, full path is must.
    -d, --foreground                  Run in the foreground
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

利用logrotate实现日志滚动
=========================

修改/etc/crontab文件，内容如下：::

    SHELL=/bin/bash
    PATH=/sbin:/bin:/usr/sbin:/usr/bin
    MAILTO=root
    HOME=/
    
    # For details see man 4 crontabs
    
    # Example of job definition:
    # .---------------- minute (0 - 59)
    # |  .------------- hour (0 - 23)
    # |  |  .---------- day of month (1 - 31)
    # |  |  |  .------- month (1 - 12) OR jan,feb,mar,apr ...
    # |  |  |  |  .---- day of week (0 - 6) (Sunday=0 or 7) OR sun,mon,tue,wed,thu,fri,sat
    # |  |  |  |  |
    # *  *  *  *  * user-name command to be executed
        ...
    */2 * * * * root /etc/cron.daily/logrotate
        ...

创建/etc/logrotate.d/itvencoder文件，内容如下：::

    /var/log/itvencoder/*log
    {
        rotate 100
        size 2M
        missingok
        notifempty
        sharedscripts
        delaycompress
        postrotate
    	/bin/kill -USR1 `cat /var/run/itvencoder.pid 2> /dev/null` 2> /dev/null || true
        endscript
    }

启动多个itvencoder
==================

由于gstreamer的pipeline在运行状态下关闭并重新启动存在如下问题，第一是有时候关闭成功需要较长时间，根据以往的log信息发现最长时间需要半个小时。
第二是有时候关闭并重新启动一个pipeline存在内存泄露。

itvencoder运行过程中，由于解码出来的音视频数据严重不同步，解码出错，编码出现问题等原因，需要重新启动pipeline，因此重启时不可避免的。

鉴于上面的原因，可以让一个通道对应一个itvencoder进程，也就是只为itvencoder配置一个通道，重启的时候不是采用streamer的相关功能，而是重新启动进程。

itvencoder提供了-c选项，可以在运行的时候指定不同的配置文件来运行多个itvencoder，这样就运行了多个频道，需要注意的是，配置文件的server部分不能冲突。

iTVEncoder安装
**************

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

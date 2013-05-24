iTVEncoder安装
==============

利用logrotate实现日志滚动
-------------------------

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

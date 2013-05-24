iTVEncoder配置文件
==================

iTVEncoder配置文件的格式充分参照了gstreamer架构的特点，采纳了gstreamer的element，bin，pipeline等概念。

iTVEncoder配置文件包括两个部分：server和channels。iTVEncoder配置文件中 "#" 之后的内容是注释，解析的时候会被忽略。

server
------

server部分包括如下内容:::
    
    [server]
    httpstreaming = 0.0.0.0:20119
    httpmgmt = 0.0.0.0:20118
    logdir = /var/log/itvencoder
    pidfile = /var/run/itvencoder.pid

httpstreaming是推流服务绑定的地址。
httpmgmt是管理接口绑定的地址。
logdir是log写入的目录。
pidfile是存储iTVEncoder进程pid的文件。

channels
--------


可修改配置项
------------

一个配置项被设为可修改，则可以通过http接口对这个配置项进行修改，而不用直接修改配置文件。通常情况下可修改配置项是针对iTVEncoder的最终用户的，比如对于输入类型为ip的源，源ip地址和端口就应该是可配置项。

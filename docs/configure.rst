iTVEncoder配置文件
******************

iTVEncoder配置文件的格式充分参照了gstreamer架构的特点，采纳了gstreamer的element，bin，pipeline等概念。

iTVEncoder配置文件包括两个部分：server和channels。iTVEncoder配置文件中 "#" 之后的内容是注释，解析的时候会被忽略。

server
======

server部分包括如下内容:::
    
    [server]
    httpstreaming = 0.0.0.0:20119
    httpmgmt = 0.0.0.0:20118
    logdir = /var/log/itvencoder

httpstreaming是推流服务绑定的地址。
httpmgmt是管理接口绑定的地址。
logdir是log写入的目录。

channels
========

channels配置的格式如下::

    [channels]
    channel_name_0 = {
       ...
    }
    channel_name_1 = {
       ...
    }
    ...

在上面的例子中channel_name_x是channel的名字，名字必须是字母数字和下划线的组合。中括号括起的部分是channel的具体定义。理论上可以配置任意多个channel。

单个channel的配置如下面的样子::

    channel_name_x = {
        enable = yes
        debug = debug option
        source = {
           ...
        }
        encoders = {
            encoder_0 = {
               ...
            }
            encoder_1 = {
               ...
            }
        }
    }

enable取值为yes或者no，yes表示启用这个通道，no表示不启用这个通道。

debug取值为一个字符串，用于配置channel进程的debug选项，具体可以参照gst-launch手册页的--gst-debug=STRING。

source表示编码通道的源，encoders表示编码通道的编码器，一个通道可以有多路编码器，编码器的名字是字母数字和下划线的组合。

source的配置如下面的样子::

    source = {
        element = {
           ...
        }

           ...

        bins = {
           ...
        }
    }

source的配置项有两类，一类是对element的配置，一类是对bins的配置。iTVEncoder解析bins，进而解析element配置，最终构造成一个gstreamer概念的pipeline，这个pipeline的输出作为encoders中的编码器的输入。

element的配置如下面的样子::

    element_name = {
        property = {
           ...
        }
        caps = gstreamer caps
        option = yes
    }

element_name是element的名字，比如x264enc，系统的gstreamer安装了那些element可以用gst-inspect命令列出来，不加参数可以列出所有已经安装的element。property用于给出element的属性，property不是必须的。caps给出该element输出的caps，格式如下::

    MIMETYPE [, PROPERTY[, PROPERTY ...]]]

option主要是用于把该element设置为可选，让用户通过web管理界面来选择是否使用这个element，因此option通常设为可配置。

bins的配置如下面的样子::

    bins = {
        bin_name = bin configuration
           ...
    }

bins定义了一组bin，这组bin构成了一个pipeline，pipeline可能是source类型的，也可能是encoder类型的。

bin的配置如下面的样子::

    bin = {
        definition = bin definition
        option = yes | no
        streaminfo = caps
    }

bin的定义与gst-launch命令中的语法格式类似。需要注意的是source的bins中需要有末端为appsink的bin，这样的bin通过appsink输出stream，iTVEncoder读取appsink输出的流交给encoder中对应的bin。encoder的bin与source的bin对应起来的方法是，encoder的bin中开头的appsrc必须有name属性，其值如果与source的某一个bin的名字匹配则两个bin就是对应的。

bin中的option指明该bin是可选的。

streaminfo用于给出该stream的元数据信息，比如对于音轨或者字幕轨，给出对应的语言类型，再比如该stream的对应的pid等。streaminfo的格式是gstreamer对的caps格式，比如::

    private/dvbsub,language=eng

或者::

    audio/mpeg,language=tha

itvencoder会检查stream类型为video和audio的stream的心跳，超时会重启通道。对于video和audio的stream，必须给出streaminfo，才会被itvencoder检查心跳。

encoder的定义与source类似，区别是source中有appsink作为末端的bin，而encoder中有appsrc作为起始的bin，还有就是encoder没有streaminfo，其streaminfo继承自对应的source的bin。

如果需要向mux传递音频或者字幕的语言信息，需要把相应的bin_name的格式按照如下定义即可::

    audio_eng
    subtitle_tha

其中下划线前面的部分指明其为音轨或者字幕，而下划线后面的部分是语言，采用ISO 639标准。

可修改配置项
============

一个配置项被设为可修改，则可以通过http接口对这个配置项进行修改，而不用直接修改配置文件。通常情况下可修改配置项是针对iTVEncoder的最终用户的，比如对于输入类型为ip的源，源ip地址和端口就应该是可配置项。

把可修改配置项放到xml格式的tag中，iTVEncoder就会自动在管理接口中提供相应的配置项::

    httpstreaming = <var name="streaming address" type="string">0.0.0.0:20119</var>

用于标示可变配置项的tag为var，需要给出两个属性，分别是name，type，相应的取值依赖于type属性。name除了标示这个配置项以外，还用于描述这个配置项的作用，在实现web管理界面的时候可以作为相应配置的label。type指出该配置项的值的类型，有四种类型的配置项，分别是string, number, option, select，string即字符串类型，number是数字型，option是布尔型，取值为TRUE和FALSE，select的格式是[baseline, main, high], 类似c中的enum类型。

实际使用中，比如url是string类型：192.168.1.1:11111。视频编码profile是select类型，[baseline, main, high]。

配置文件实例
============

在源代码中conf目录下有配置文件的实例。目前有如下配置文件实例::

    itvencoder.conf, 多个通道的配置文件
    itvencoder.conf.ac3.ip，针对源音频编码为ac3的配置文件
    itvencoder.conf.ip， 针对输入源为ip的配置文件。
    itvencoder.conf.subtitle， 针对多音轨多字幕的源的配置文件。
    itvencoder.conf.subtitleoverlay，针对字幕叠加的配置文件。
    itvencoder.conf.test，源为videotestsrc和audiotestsrc的配置文件，这个配置文件不需要特殊的环境，安装了gstreamer即可测试体验itvencoder。
    itvencoder.conf.v4l2，源为支持v4l2的采集卡的配置文件。

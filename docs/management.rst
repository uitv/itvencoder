管理接口
========

获取当前配置
------------

HTTP Request::

    GET /configure/ HTTP/1.1
    Host: 192.168.2.9:20118
    Connection: keep-alive
    Cache-Control: max-age=0
    User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/27.0.1453.93 Safari/537.36
    Accept: */*
    Referer: http://192.168.2.9:20118/mgmt/
    Accept-Encoding: gzip,deflate,sdch
    Accept-Language: zh-CN,zh;q=0.8
    
HTTP Response::

    <?xml version="1.0" encoding="utf-8"?>
    <root>
        <server>
            <var name="streaming address" id="10" type="string">0.0.0.0:20119</var>
            <var name="management address" id="15" type="string">0.0.0.0:20118</var>
        </server>
        <channels>
            <channel0>
                <var name="enable" id="27" type="option">yes</var>
                <source>
                    <var name="textoverlay option" id="40" type="option">yes</var>
                    <var name="udpsrc uri" id="48" type="string">udp://239.100.191.8:1234</var>
                    <var name="program number" id="58" type="number">133</var>
                    <var name="video pid" id="108" type="string">video_0532</var>
                    <var name="video decoder" id="110" type="[mpeg2dec, ffdec_mpeg2video]">ffdec_mpeg2video</var>
                    <var name="audio1 pid" id="115" type="string">audio_0533</var>
                    <var name="select audio1" id="118" type="option">yes</var>
                    <var name="audio2 pid" id="123" type="string">audio_0271</var>
                    <var name="select audio2" id="126" type="option">no</var>
                </source>
                <encoder>
                    <encoder1>
                        <var name="width" id="143" type="number">720</var>
                        <var name="height" id="145" type="number">576</var>
                        <var name="bitrate" id="154" type="number">1500</var>
                        <var name="b frames" id="168" type="number">3</var>
                        <var name="profile" id="171" type="[baseline, main, high]">high</var>
                        <var name="select audio1" id="211" type="option">yes</var>
                        <var name="select audio2" id="217" type="option">no</var>
                    </encoder1>
                    <encoder2>
                        <var name="width" id="235" type="number">720</var>
                        <var name="height" id="237" type="number">576</var>
                        <var name="bitrate" id="246" type="number">800</var>
                        <var name="b frames" id="260" type="number">3</var>
                        <var name="profile" id="263" type="[baseline, main, high]">high</var>
                        <var name="select audio1" id="303" type="option">yes</var>
                        <var name="select audio2" id="309" type="option">no</var>
                    </encoder2>
                </encoder>
            </channel0>
        </channels>
    </root>

存储配置
--------

对可变配置项进行修改后，需要存储到配置文件中，对应的请求如下

HTTP Request::

    POST /configure/ HTTP/1.1
    Host: 192.168.2.9:20118
    Connection: keep-alive
    Content-Length: 2383
    Origin: http://192.168.2.9:20118
    User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/27.0.1453.93 Safari/537.36
    Content-Type: application/xml
    Accept: */*
    Referer: http://192.168.2.9:20118/mgmt/
    Accept-Encoding: gzip,deflate,sdch
    Accept-Language: zh-CN,zh;q=0.8
    
    <?xml version="1.0" encoding="utf-8"?><root>
        <server>
            <var name="streaming address" id="10" type="string">0.0.0.0:20119</var>
            <var name="management address" id="15" type="string">0.0.0.0:20118</var>
        </server>
        <channels>
            <channel0>
                <var name="enable" id="27" type="option">yes</var>
                <source>
                    <var name="textoverlay option" id="40" type="option">yes</var>
                    <var name="udpsrc uri" id="48" type="string">udp://239.100.191.8:1234</var>
                    <var name="program number" id="58" type="number">133</var>
                    <var name="video pid" id="108" type="string">video_0532</var>
                    <var name="video decoder" id="110" type="[mpeg2dec, ffdec_mpeg2video]">ffdec_mpeg2video</var>
                    <var name="audio1 pid" id="115" type="string">audio_0533</var>
                    <var name="select audio1" id="118" type="option">yes</var>
                    <var name="audio2 pid" id="123" type="string">audio_0271</var>
                    <var name="select audio2" id="126" type="option">no</var>
                </source>
                <encoder>
                    <encoder1>
                        <var name="width" id="143" type="number">720</var>
                        <var name="height" id="145" type="number">576</var>
                        <var name="bitrate" id="154" type="number">1500</var>
                        <var name="b frames" id="168" type="number">3</var>
                        <var name="profile" id="171" type="[baseline, main, high]">high</var>
                        <var name="select audio1" id="211" type="option">yes</var>
                        <var name="select audio2" id="217" type="option">no</var>
                    </encoder1>
                    <encoder2>
                        <var name="width" id="235" type="number">720</var>
                        <var name="height" id="237" type="number">576</var>
                        <var name="bitrate" id="246" type="number">800</var>
                        <var name="b frames" id="260" type="number">3</var>
                        <var name="profile" id="263" type="[baseline, main, high]">high</var>
                        <var name="select audio1" id="303" type="option">yes</var>
                        <var name="select audio2" id="309" type="option">no</var>
                    </encoder2>
                </encoder>
            </channel0>
        </channels>
    </root>

HTTP Response::
    
    HTTP/1.1 200 Ok
    Server: iTVEncoder-0.3.0
    Content-Type: text/plain
    Content-Length: 2
    Connection: Close
    
    Ok

重新启动iTVEncoder
--------------------------

HTTP Request::

    GET /kill HTTP/1.1
    Host: 192.168.2.9:20118
    Connection: keep-alive
    User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/27.0.1453.93 Safari/537.36
    Accept: */*
    Referer: http://192.168.2.9:20118/mgmt/
    Accept-Encoding: gzip,deflate,sdch
    Accept-Language: zh-CN,zh;q=0.8

重新启动的请求没有应答，可以通过尝试连接管理接口来判断是否已经启动完成。

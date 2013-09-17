import time
import httplib

def request(server, port, channelid, command):
    conn = httplib.HTTPConnection(server, port)
    channel = "/channel/%d/%s" % (channelid, command)
    conn.request("GET", channel)
    resp = conn.getresponse()
    if resp.status == 200:
        data = resp.read (16)
    conn.close()

def save (server, port):
    conn = httplib.HTTPConnection(server, port)
    params = """
<?xml version="1.0" encoding="utf-8"?>
<root>
    <server>
        <var name="streaming address" id="10" type="string">0.0.0.0:20119</var>
        <var name="management address" id="15" type="string">0.0.0.0:20118</var>
    </server>
    <channels>
        <channel name="channel0">
            <var name="enable" id="27" type="option">yes</var>
            <source>
                <var name="udp source address" id="36" type="string">udp://127.0.0.1:6001</var>
                <var name="video decoder" id="68" type="[h264dec, ffdec_mpeg2video]">ffdec_mpeg2video</var>
                <var name="audio decoder" id="73" type="[mad, ffdec_ac3]">mad</var>
            </source>
            <encoders>
                <encoder name="encoder1">
                    <var name="width" id="90" type="number">720</var>
                    <var name="height" id="92" type="number">576</var>
                    <var name="bitrate" id="101" type="number">1000</var>
                    <var name="profile" id="116" type="[baseline, main, high]">high</var>
                </encoder>
                <encoder name="encoder2">
                    <var name="width" id="164" type="number">720</var>
                    <var name="height" id="166" type="number">576</var>
                    <var name="bitrate" id="175" type="number">1200</var>
                    <var name="profile" id="190" type="[baseline, main, high]">high</var>
                </encoder>
            </encoders>
        </channel>
        <channel name="channel1">
            <var name="enable" id="234" type="option">yes</var>
            <source>
                <var name="udpsrc uri" id="242" type="string">udp://127.0.0.1:6003</var>
                <var name="video pid" id="273" type="string">video_0201</var>
                <var name="video decoder" id="275" type="[h264dec, ffdec_mpeg2video]">h264dec</var>
            </source>
            <encoders>
                <encoder name="encoder1">
                    <var name="width" id="296" type="number">1280</var>
                    <var name="height" id="298" type="number">720</var>
                    <var name="bitrate" id="307" type="number">1500</var>
                    <var name="profile" id="322" type="[baseline, main, high]">high</var>
                </encoder>
                <encoder name="encoder2">
                    <var name="width" id="369" type="number">720</var>
                    <var name="height" id="371" type="number">576</var>
                    <var name="bitrate" id="380" type="number">1200</var>
                    <var name="profile" id="395" type="[baseline, main, high]">high</var>
                </encoder>
            </encoders>
        </channel>
        <channel name="channel2">
            <var name="enable" id="438" type="option">yes</var>
            <source>
                <var name="udp source address" id="446" type="string">udp://127.0.0.1:6002</var>
                <var name="video pid" id="478" type="string">video_18c4</var>
                <var name="video decoder" id="480" type="[h264dec, ffdec_mpeg2video]">ffdec_mpeg2video</var>
                <var name="audio pid" id="485" type="string">audio_18c5</var>
                <var name="audio decoder" id="487" type="[mad, ffdec_ac3]">ffdec_ac3</var>
            </source>
            <encoders>
                <encoder name="encoder1">
                    <var name="width" id="504" type="number">1280</var>
                    <var name="height" id="506" type="number">720</var>
                    <var name="bitrate" id="515" type="number">1500</var>
                    <var name="profile" id="530" type="[baseline, main, high]">high</var>
                </encoder>
                <encoder name="encoder2">
                    <var name="width" id="578" type="number">720</var>
                    <var name="height" id="580" type="number">576</var>
                    <var name="bitrate" id="589" type="number">1200</var>
                    <var name="profile" id="604" type="[baseline, main, high]">high</var>
                </encoder>
            </encoders>
        </channel>
        <channel name="channel3">
            <var name="enable" id="648" type="option">yes</var>
            <source>
                <var name="udpsrc uri" id="656" type="string">udp://225.1.1.31:1234</var>
                <var name="video pid" id="687" type="string">video_0121</var>
                <var name="video decoder" id="689" type="[h264dec, ffdec_mpeg2video]">h264dec</var>
            </source>
            <encoders>
                <encoder name="encoder1">
                    <var name="width" id="710" type="number">1280</var>
                    <var name="height" id="712" type="number">720</var>
                    <var name="bitrate" id="721" type="number">1200</var>
                    <var name="profile" id="736" type="[baseline, main, high]">high</var>
                </encoder>
                <encoder name="encoder2">
                    <var name="width" id="783" type="number">720</var>
                    <var name="height" id="785" type="number">576</var>
                    <var name="bitrate" id="794" type="number">1200</var>
                    <var name="profile" id="809" type="[baseline, main, high]">high</var>
                </encoder>
            </encoders>
        </channel>
        <channel name="channel4">
            <var name="enable" id="852" type="option">yes</var>
            <source>
                <var name="udp source address" id="860" type="string">udp://225.1.1.31:1234</var>
                <var name="video pid" id="892" type="string">video_0121</var>
                <var name="video decoder" id="894" type="[h264dec, ffdec_mpeg2video]">h264dec</var>
                <var name="audio pid" id="899" type="string">audio_0122</var>
                <var name="audio decoder" id="901" type="[mad, ffdec_ac3]">mad</var>
            </source>
            <encoders>
                <encoder name="encoder1">
                    <var name="width" id="918" type="number">1280</var>
                    <var name="height" id="920" type="number">720</var>
                    <var name="bitrate" id="929" type="number">1500</var>
                    <var name="profile" id="944" type="[baseline, main, high]">high</var>
                </encoder>
                <encoder name="encoder2">
                    <var name="width" id="992" type="number">720</var>
                    <var name="height" id="994" type="number">576</var>
                    <var name="bitrate" id="1003" type="number">1200</var>
                    <var name="profile" id="1018" type="[baseline, main, high]">high</var>
                </encoder>
            </encoders>
        </channel>
        <channel name="channel5">
            <var name="enable" id="1062" type="option">yes</var>
            <source>
                <var name="udpsrc uri" id="1070" type="string">udp://127.0.0.1:6008</var>
                <var name="video pid" id="1101" type="string">video_0201</var>
                <var name="video decoder" id="1103" type="[h264dec, ffdec_mpeg2video]">h264dec</var>
            </source>
            <encoders>
                <encoder name="encoder1">
                    <var name="width" id="1124" type="number">1280</var>
                    <var name="height" id="1126" type="number">720</var>
                    <var name="bitrate" id="1135" type="number">1500</var>
                    <var name="profile" id="1150" type="[baseline, main, high]">high</var>
                </encoder>
                <encoder name="encoder2">
                    <var name="width" id="1197" type="number">720</var>
                    <var name="height" id="1199" type="number">576</var>
                    <var name="bitrate" id="1208" type="number">1200</var>
                    <var name="profile" id="1223" type="[baseline, main, high]">high</var>
                </encoder>
            </encoders>
        </channel>
        <channel name="channel6">
            <var name="enable" id="1266" type="option">yes</var>
            <source>
                <var name="udp source address" id="1274" type="string">udp://127.0.0.1:6002</var>
                <var name="video pid" id="1306" type="string">video_18c4</var>
                <var name="video decoder" id="1308" type="[h264dec, ffdec_mpeg2video]">ffdec_mpeg2video</var>
                <var name="audio pid" id="1313" type="string">audio_18c5</var>
                <var name="audio decoder" id="1315" type="[mad, ffdec_ac3]">ffdec_ac3</var>
            </source>
            <encoders>
                <encoder name="encoder1">
                    <var name="width" id="1332" type="number">1280</var>
                    <var name="height" id="1334" type="number">720</var>
                    <var name="bitrate" id="1343" type="number">1500</var>
                    <var name="profile" id="1358" type="[baseline, main, high]">main</var>
                </encoder>
                <encoder name="encoder2">
                    <var name="width" id="1406" type="number">720</var>
                    <var name="height" id="1408" type="number">576</var>
                    <var name="bitrate" id="1417" type="number">1200</var>
                    <var name="profile" id="1432" type="[baseline, main, high]">high</var>
                </encoder>
            </encoders>
        </channel>
        <channel name="channel7">
            <var name="enable" id="1476" type="option">yes</var>
            <source>
                <var name="udpsrc uri" id="1484" type="string">udp://127.0.0.1:6003</var>
                <var name="video pid" id="1515" type="string">video_0201</var>
                <var name="video decoder" id="1517" type="[h264dec, ffdec_mpeg2video]">h264dec</var>
            </source>
            <encoders>
                <encoder name="encoder1">
                    <var name="width" id="1538" type="number">1280</var>
                    <var name="height" id="1540" type="number">720</var>
                    <var name="bitrate" id="1549" type="number">1500</var>
                    <var name="profile" id="1564" type="[baseline, main, high]">high</var>
                </encoder>
                <encoder name="encoder2">
                    <var name="width" id="1611" type="number">720</var>
                    <var name="height" id="1613" type="number">576</var>
                    <var name="bitrate" id="1622" type="number">1200</var>
                    <var name="profile" id="1637" type="[baseline, main, high]">high</var>
                </encoder>
            </encoders>
        </channel>
        <channel name="channel8">
            <var name="enable" id="1680" type="option">yes</var>
            <source>
                <var name="udp source address" id="1688" type="string">udp://127.0.0.1:6002</var>
                <var name="video pid" id="1720" type="string">video_18c4</var>
                <var name="video decoder" id="1722" type="[h264dec, ffdec_mpeg2video]">ffdec_mpeg2video</var>
                <var name="audio pid" id="1727" type="string">audio_18c5</var>
                <var name="audio decoder" id="1729" type="[mad, ffdec_ac3]">ffdec_ac3</var>
            </source>
            <encoders>
                <encoder name="encoder1">
                    <var name="width" id="1746" type="number">1280</var>
                    <var name="height" id="1748" type="number">720</var>
                    <var name="bitrate" id="1757" type="number">1500</var>
                    <var name="profile" id="1772" type="[baseline, main, high]">high</var>
                </encoder>
                <encoder name="encoder2">
                    <var name="width" id="1820" type="number">720</var>
                    <var name="height" id="1822" type="number">576</var>
                    <var name="bitrate" id="1831" type="number">1200</var>
                    <var name="profile" id="1846" type="[baseline, main, high]">high</var>
                </encoder>
            </encoders>
        </channel>
        <channel name="channel9">
            <var name="enable" id="1890" type="option">yes</var>
            <source>
                <var name="udpsrc uri" id="1898" type="string">udp://127.0.0.1:6003</var>
                <var name="video pid" id="1929" type="string">video_0201</var>
                <var name="video decoder" id="1931" type="[h264dec, ffdec_mpeg2video]">h264dec</var>
            </source>
            <encoders>
                <encoder name="encoder1">
                    <var name="width" id="1952" type="number">1280</var>
                    <var name="height" id="1954" type="number">720</var>
                    <var name="bitrate" id="1963" type="number">1500</var>
                    <var name="profile" id="1978" type="[baseline, main, high]">high</var>
                </encoder>
                <encoder name="encoder2">
                    <var name="width" id="2025" type="number">720</var>
                    <var name="height" id="2027" type="number">576</var>
                    <var name="bitrate" id="2036" type="number">1200</var>
                    <var name="profile" id="2051" type="[baseline, main, high]">high</var>
                </encoder>
            </encoders>
        </channel>
    </channels>
</root>
    """
    headers = {
        "Content-Type": "application/xml",
        "Content-Length": 4586
    }
    conn.request("POST", "/configure", params, headers)
    response = conn.getresponse()
    print response.status, response.reason

i = 0
while True:
    save ("192.168.2.10", 20118)
    time.sleep(1)
    """
    print "stop %d" % i
    request("192.168.2.10", 20118, 0, "stop")
    request("192.168.2.10", 20118, 1, "stop")
    request("192.168.2.10", 20118, 2, "stop")
    request("192.168.2.10", 20118, 3, "stop")
    request("192.168.2.10", 20118, 4, "stop")
    request("192.168.2.10", 20118, 5, "stop")
    request("192.168.2.10", 20118, 6, "stop")
    request("192.168.2.10", 20118, 7, "stop")
    request("192.168.2.10", 20118, 8, "stop")
    request("192.168.2.10", 20118, 9, "stop")
    request("192.168.2.10", 20118, 0, "start")
    request("192.168.2.10", 20118, 1, "start")
    request("192.168.2.10", 20118, 2, "start")
    request("192.168.2.10", 20118, 3, "start")
    request("192.168.2.10", 20118, 4, "start")
    request("192.168.2.10", 20118, 5, "start")
    request("192.168.2.10", 20118, 6, "start")
    request("192.168.2.10", 20118, 7, "start")
    request("192.168.2.10", 20118, 8, "start")
    request("192.168.2.10", 20118, 9, "start")
    request("192.168.2.10", 20118, 0, "restart")
    request("192.168.2.10", 20118, 1, "restart")
    time.sleep(1)
    request("192.168.2.10", 20118, 2, "restart")
    request("192.168.2.10", 20118, 3, "restart")
    time.sleep(1)
    request("192.168.2.10", 20118, 4, "restart")
    request("192.168.2.10", 20118, 5, "restart")
    time.sleep(1)
    request("192.168.2.10", 20118, 6, "restart")
    request("192.168.2.10", 20118, 7, "restart")
    time.sleep(1)
    request("192.168.2.10", 20118, 8, "restart")
    request("192.168.2.10", 20118, 9, "restart")
    #request("192.168.2.10", 20118, 2, "start")
    #request("192.168.2.10", 20118, 1, "start")
    #time.sleep(600)
    i = i+1
    """

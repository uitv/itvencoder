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

i = 0
while True:
    print "stop %d" % i
    """
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
    """
    time.sleep(27)
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

import time
import httplib

def restart(server, port, channelid):
    conn = httplib.HTTPConnection(server, port)
    channel = "/channel/%d/restart" % channelid
    conn.request("GET", channel)
    resp = conn.getresponse()
    if resp.status == 200:
        data = resp.read (16)
    conn.close()

def stop(server, port, channelid):
    conn = httplib.HTTPConnection(server, port)
    channel = "/channel/%d/stop" % channelid
    conn.request("GET", channel)
    resp = conn.getresponse()
    if resp.status == 200:
        data = resp.read(16)
    conn.close()

i = 0
while True:
    print "restart %d" % i
    restart("192.168.2.10", 20118, 0)
    time.sleep(5)
    restart("192.168.2.10", 20118, 1)
    time.sleep(5)
    i = i+1

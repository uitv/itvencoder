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
    request("192.168.2.10", 20118, 2, "stop")
    time.sleep(2)
    request("192.168.2.10", 20118, 2, "start")
    time.sleep(30)
    i = i+1

import httplib
import time

def fetch_segment(server, port, location, size):
    #conn = httplib.HTTPConnection("192.168.2.11:20129")
    t1 = time.time()
    conn = httplib.HTTPConnection("%s:%s" % (server, port))
    conn.request("GET", location)
    resp = conn.getresponse()
    if resp.status == 200:
        data = resp.read (size)
    conn.close
    t2 = time.time()
    if t2-t1 > 10:
        print "server: ", server, location, "status: ", resp.status, resp.reason, "time:", t2-t1, "read size: ", len(data)

while True:
    fetch_segment("192.168.2.11", "20129", "/channel/0/encoder/0", 198*1024)
    fetch_segment("192.168.2.11", "20129", "/channel/0/encoder/1", 198*1024)
    fetch_segment("192.168.2.11", "20129", "/channel/1/encoder/0", 198*1024)
    fetch_segment("192.168.2.11", "20129", "/channel/1/encoder/1", 198*1024)
    fetch_segment("192.168.2.11", "20129", "/channel/2/encoder/0", 198*1024)
    fetch_segment("192.168.2.11", "20129", "/channel/2/encoder/1", 198*1024)

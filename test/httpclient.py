import socket
import httplib
import time
import string

ReadSize = 190*1024

def fetch_data(server, port, location, size):
    attempts = 5
    while True:
        try:
            t1 = time.time()
            conn = httplib.HTTPConnection(server, port)
            conn.request("GET", location)
            resp = conn.getresponse()
            if resp.status == 200:
                data = resp.read (size)
                t2 = time.time()
                conn.close()
                if not (len(data) == ReadSize) or t2-t1 > 1:
                    print time.strftime("%b %d - %H:%M:%S", time.gmtime()), "http://%s%s" % (server, location), resp.status, resp.reason, "size", len(data), "time", t2-t1
            break
        except socket.error, msg:
            print "socket error %s" % msg
        except httplib.HTTPException, msg:
            print time.strftime("%b %d - %H:%M:%S", time.gmtime()), "read data %s:%d%s, error %s" % (server, port, location, msg)
            attempts -= 1
            if attempts == 0:
                raise
            else:
                print "try %d times" % (5-attempts)
                time.sleep(1)
                continue

while True:
    fetch_data("192.168.2.11", 20129, "/channel/0/encoder/0", ReadSize)
    fetch_data("192.168.2.11", 20129, "/channel/0/encoder/1", ReadSize)
    fetch_data("192.168.2.11", 20129, "/channel/1/encoder/0", ReadSize)
    fetch_data("192.168.2.11", 20129, "/channel/1/encoder/1", ReadSize)
    fetch_data("192.168.2.11", 20129, "/channel/2/encoder/0", ReadSize)
    fetch_data("192.168.2.11", 20129, "/channel/2/encoder/1", ReadSize)

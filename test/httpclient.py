import httplib
import time
import string

ReadSize = 1900*1024

def decode_chunked(data):
    offset = 0
    offset = string.index(data, '\r\n') + 4
    print offset

def fetch_data(server, port, location, size):
    t1 = time.time()
    try:
        conn = httplib.HTTPConnection(server, port)
    except:
        print time.strftime("%b %d - %H:%M:%S", time.gmtime()), "Connect %s:%d error" % (server, port)
        return

    try:
        conn.request("GET", location)
    except:
        print time.strftime("%b %d - %H:%M:%S", time.gmtime()), "GET %s:%d%s error" % (server, port, location)
        conn.close
        return

    try:
        resp = conn.getresponse()
    except:
        print time.strftime("%b %d - %H:%M:%S", time.gmtime()), "get response %s:%d%s error" % (server, port, location)
        conn.close()
        return

    if resp.status == 200:
        try:
            data = resp.read (size)
            t2 = time.time()
            if t2-t1>9:
                name = time.strftime("%b%d%H%M%S", time.gmtime())
                open(name, "w+").write(data)
                data = resp.read(1024*1024*2)
                open(name).wirte(data)
        except:
            print time.strftime("%b %d - %H:%M:%S", time.gmtime()), "read data %s:%d%s error" % (server, port, location)
            conn.close
            return
        conn.close
        t2 = time.time()
        if not (len(data) == ReadSize) or t2-t1 > 2:
            print time.strftime("%b %d - %H:%M:%S", time.gmtime()), "http://%s%s" % (server, location), resp.status, resp.reason, "size", len(data), "time", t2-t1
            #open(time.strftime("%b%d%H%M%S", time.gmtime()), 'w+').write(data)
    else:
        print time.strftime("%b %d - %H:%M:%S", time.gmtime()), "http://%s%s" % (server, location), resp.status, resp.reason

while True:
    fetch_data("192.168.2.11", 20129, "/channel/0/encoder/0", ReadSize)
    fetch_data("192.168.2.11", 20129, "/channel/0/encoder/1", ReadSize)
    fetch_data("192.168.2.11", 20129, "/channel/1/encoder/0", ReadSize)
    fetch_data("192.168.2.11", 20129, "/channel/1/encoder/1", ReadSize)
    fetch_data("192.168.2.11", 20129, "/channel/2/encoder/0", ReadSize)
    fetch_data("192.168.2.11", 20129, "/channel/2/encoder/1", ReadSize)

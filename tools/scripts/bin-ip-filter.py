#!/usr/bin/env python
import sys,re,socket,struct

# Only take cares one occurence per line, but should be enough
re_hex_ip = re.compile("([0-9a-f]{8})")

for l in sys.stdin:
    res = re_hex_ip.search(l)
    
    if res:
        ip = socket.inet_ntoa(struct.pack("!I", int(res.group(1),16)))
        l = re_hex_ip.sub(ip, l)

    sys.stdout.write(l)
        

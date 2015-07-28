#!/usr/bin/env python
import sys

res = dict()
for l in sys.stdin:
    if l.startswith("%"):
        continue
    e = l.strip().split(",")
    if res.has_key(e[0]):
        res[e[0]].update(set(e[1:]))
    else:
        res.setdefault(e[0], set(e[1:]))

for k, v in res.iteritems():
    sys.stdout.write(k)
    for ip in v:
        sys.stdout.write(",")
        sys.stdout.write(ip)

    sys.stdout.write("\n")
    

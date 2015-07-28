#!/usr/bin/env python
import parsing_utils
import sys
import struct
import socket

data = []
dest = dict()

for l in sys.stdin:
    data.append(l)
    e = l.strip().split(",")
    if e[2] != "to-be-fixed":
        dest.setdefault(e[0],e[2])

for l in data:
    e = l.strip().split(",")
    if e[2] == "to-be-fixed":
        found = False
        for rp, if_name in dest.items():
            if ip_in_prefix(e[1], rp):
                found = True
                l = l.replace(",to-be-fixed,", ",%s," % if_name)
                break

        if not found:
            print "Unhandled interface for line %s" %l

    sys.stdout.write(l)

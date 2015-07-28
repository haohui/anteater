#!/usr/bin/env python

import sys

used_ips = set()
dup_ips = set()
router_ips = []

for l in sys.stdin:
    e = l.strip().split(",")
    f = e[1:]
    router_ips.append((e[0], f))
    for ip in f:
        if ip in used_ips:
	    dup_ips.add(ip)
	else:
            used_ips.add(ip)

for l in router_ips:
    ip_list = filter(lambda x : x not in dup_ips, l[1])
    ip_list.sort()
    if len(l[1]) > 0:
        sys.stdout.write(l[0])
        for ip in ip_list:
            sys.stdout.write(',')
            sys.stdout.write(ip)
        sys.stdout.write("\n")
    else:
        print "%%s" % l[0]

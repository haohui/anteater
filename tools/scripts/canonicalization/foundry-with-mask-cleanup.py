#!/usr/bin/env python

import parsing_utils
import sys

for l in sys.stdin:
    e = l.strip().split(",")
    if e[2] == "0.0.0.0" or e[2] == "255.255.255.255":
	e[2] = "DIRECT"
    print "%s/%d,%s,%s,%s" % (e[0], parsing_utils.netmask_to_width(e[1]), e[2].replace("*", ""), e[3], e[4])

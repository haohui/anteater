#!/usr/bin/env python

import sys

d = dict()

for l in sys.stdin:
    f = l.strip().split(",")
    if d.has_key(f[0]):
        d[f[0]].append(f[1])
    else:
        d.setdefault(f[0], [f[1]])

for k, v in d.iteritems():
    sys.stdout.write(k)
    sys.stdout.write(",")

    for q in xrange(0, len(v)-2):
        sys.stdout.write(v[q])
        sys.stdout.write(",")

    print v[len(v)-1]
    
        

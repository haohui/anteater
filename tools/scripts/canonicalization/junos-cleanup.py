#!/usr/bin/env python
import sys

prev_line = ""
for l in sys.stdin:
    ls = l.strip()
    e = ls.split(",")
    if len(e[0]) == 0:
        sys.stdout.write(prev_line)
        sys.stdout.write(l)
    else:
        prev_line = ls

#!/usr/bin/env python

import parsing_utils
import sys
import socket
import struct

class router:
    def __init__(self, name):
        self.name = name
        self.prefixes_if_mapping = dict()
        self.owned_prefixes = dict()
        self.nexthops = dict()
        self.owned_ip = set()
        self.if_nh_map = dict()

    def parse(self, fn):
        f = open(fn, 'r')
        for l in f:
            if not l.startswith("%"):
                self.parse_line(l)

    def parse_line(self, l):
        e = l.strip().split(",")
        if e[1] == "DIRECT":
            if e[2].startswith("loopback"):
                self.owned_ip.add(e[0].split("/")[0])
            if e[0].endswith("/30"):
                self.prefixes_if_mapping.setdefault(e[0], e[2])
        elif e[1] != "DIRECT":
            self.nexthops.setdefault(e[1], e[2])
            if not self.if_nh_map.has_key(e[2]):
                self.if_nh_map.setdefault(e[2], set())

            self.if_nh_map[e[2]].add(e[1])

    def solve(self):
        for rp, if_name in self.prefixes_if_mapping.items():
            l = self.if_nh_map.get(if_name, [])
            # Only deal with the cases where there's one-to-one mapping between interface and ip
            if len(l) == 1:
                self.owned_prefixes.setdefault(rp, list(self.if_nh_map[if_name])[0])

router_list = []

#for l in open("fib-list.txt", 'r'):
for l in sys.stdin:
    if l.startswith("%"):
        continue

    e = l.strip().split(",")
    r = router(e[0])
    r.parse(e[1])
    r.solve()
    
    router_list.append(r)

for i in xrange(0, len(router_list)):
   for j in xrange(0, len(router_list)):
      if i == j:
          continue

      for nh in router_list[j].nexthops.iterkeys():
          for rp,self_ip in router_list[i].owned_prefixes.items():
              # Vague data
              if nh == self_ip:
                  continue
              if parsing_utils.ip_in_prefix(nh, rp):
                  router_list[i].owned_ip.add(nh)
                  router_list[j].owned_ip.add(self_ip)

# Verifier
used_ips = set()
for r in router_list:
    existed_ips = used_ips & r.owned_ip
    used_ips.update(r.owned_ip)
    if len(existed_ips) != 0:
        print "%%WARN: Duplicated IPs for %s" % existed_ips



for r in router_list:
    if len(r.owned_ip) == 0:
        sys.stdout.write("%")
    sys.stdout.write(r.name)
    for ip in r.owned_ip:
        sys.stdout.write(",")
        sys.stdout.write(ip)
    sys.stdout.write("\n")
    

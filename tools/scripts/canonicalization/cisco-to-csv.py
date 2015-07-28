#!/usr/bin/env python
import sys
import re
import math

def warn(s):
    print "WARN: " + s

def flag_tr(s):
    return '|'.join(s.replace("*", " ").replace("O IA", "Oi").replace("O E2", "O2").replace("C", "D").strip().split(" "))

def if_tr(s):
    return s.replace("Loopback", "loopback")

class cisco_parser:
    state_default_route = 0
    state_parse_fib = 1
    state_parse_eq_subnet = 2
    re_default_route = re.compile("Gateway of last resort is ([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+) to network ([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)")
    re_eq_subnetted = re.compile("[ ]+ [0-9]+\.[0-9]+\.[0-9]+\.[0-9]+/([0-9]+) is subnetted, [0-9]+ subnets")
    re_var_subnetted = re.compile("[ ]+ [0-9]+\.[0-9]+\.[0-9]+\.[0-9]+/[0-9]+ is variably subnetted, [0-9]+ subnets, [0-9]+ masks")
    re_subnet_entry = re.compile("([A-Za-z0-9* ]+)? ([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)?(/[0-9]+)?[ ]?\[[0-9]+/[0-9]+\] via ([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+), [^,]+, ([A-Za-z0-9/]+)")
    re_direct_connected = re.compile("([A-Za-z0-9* ]+) ([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)(/[0-9]+)? is directly connected, ([A-Za-z0-9/]+)")
    re_static_route = re.compile("([A-Za-z0-9* ]+) ([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)(/[0-9]+)? \[[0-9]+/[0-9]+\] via ([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)")

    def __init__(self):
        self.current_state = 0
        self.prev_tag = ""
        self.prev_rp_ip = ""
        self.prev_rp_width = ""

    def set_state(self, s):
        self.current_state = s;
    
    def parse_default_route(self, line):
        res = cisco_parser.re_default_route.match(line)
        if res != None:
            # A default route is in the FIBs though
#            print "%s/0,%s,to-be-fixed," % (res.group(2), res.group(1))
            self.set_state(cisco_parser.state_parse_fib)
            return True
        return False

    def parse_fib(self, line):
        # If the function fails, it tries to parse static routes
        # It if succeeds, it's going to parse FIB entry, so set the state anyway
        self.set_state(cisco_parser.state_parse_eq_subnet)
        res = cisco_parser.re_eq_subnetted.match(line)
        
        if res != None:
            self.prev_rp_width = res.group(1)
            return True
    
        res = cisco_parser.re_var_subnetted.match(line)
        if res != None:
            return True
            
        return False

    def parse_eq_subnet_entry(self, line):
        res = cisco_parser.re_subnet_entry.match(line)
        if res != None:
            if res.group(1):
                if res.group(1).strip():
                    self.prev_tag = flag_tr(res.group(1))

            if res.group(2):
                self.prev_rp_ip = res.group(2)

            if res.group(3):
                self.prev_rp_width = res.group(3).strip("/")

            print "%s/%s,%s,%s,%s" % (self.prev_rp_ip, self.prev_rp_width, res.group(4), if_tr(res.group(5)), self.prev_tag)
            return True

        res = cisco_parser.re_direct_connected.match(line)
        if res != None:
            if res.group(3):
                self.prev_rp_width = res.group(3).strip("/")

            print "%s/%s,DIRECT,%s,%s" % (res.group(2), self.prev_rp_width, if_tr(res.group(4)), flag_tr(res.group(1)))
            return True
            
        res = cisco_parser.re_static_route.match(line)
        if res != None:
            if res.group(3):
                self.prev_rp_width = res.group(3).strip("/")

            print "%s/%s,%s,to-be-fixed,%s" % (res.group(2), self.prev_rp_width, res.group(4), flag_tr(res.group(1)))
            return True
        

        # Try out luck again on FIB
        self.set_state(cisco_parser.state_parse_fib)
        return False


    def parse(self, line):
        parse_time = 0
        while parse_time < 2:
            parse_time += 1
            if self.current_state == cisco_parser.state_default_route:
                ret = self.parse_default_route(line)
            elif self.current_state == cisco_parser.state_parse_fib:
                ret = self.parse_fib(line)
            elif self.current_state == cisco_parser.state_parse_eq_subnet:
                ret = self.parse_eq_subnet_entry(line)

            if ret:
                return

        print "Error: Prasing error, state=%d, line=%s" % (self.current_state, line)
        return
                        

p = cisco_parser()
for l in sys.stdin:
    l = l.rstrip()
    if len(l.strip()) != 0:
        p.parse(l)
    

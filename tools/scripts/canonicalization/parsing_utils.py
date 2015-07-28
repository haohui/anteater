import struct
import socket

def ip_in_prefix(ip, rp):
    e = rp.split("/")
    rp_ip = struct.unpack("!I", socket.inet_aton(e[0]))[0]
    rp_width = int(e[1])
    assert rp_width == 30
    ip_in = struct.unpack("!I", socket.inet_aton(ip))[0]
    return ip_in & (~3) == rp_ip

def netmask_to_width(mask):
    mask_ip = struct.unpack("!I", socket.inet_aton(mask))[0]
    s = bin(mask_ip).lstrip("-0b")
    return len(s.rstrip("0"))
    

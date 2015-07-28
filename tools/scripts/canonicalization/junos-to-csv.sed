/^inet\.1:/,$ { d }
/^inet6/,$ { d }
/^224\./d
/^[0-9]\| to \| via\|Reject/!d
 s/> to //g; s/ to //g; s/\[\(.*\)\].*$/\1/g; s/ \+\*/,/g ; s/ via \(.*\)$/,\1/g ; s/^ \+/,/g ;
# Connection Tags
s/OSPF\/[0-9]\+/O/g; s/Direct\/[0-9]\+/D/g; s/Static\/[0-9]\+/S/g; s/Local\/[0-9]\+/DL/g;
# Connection
s/,Local,\([^,]*\)$/,DIRECT,loopback-\1/g; s/,>,/,DIRECT,/g; s/Reject/DIRECT,drop/g;
# Interface Name
s/,lo\([^,]*\)$/,loopback\1/g;

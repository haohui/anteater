#!/usr/bin/env bash

for i in `find vanilla/cisco-out-int vanilla/foundry-out-int -type f`; do
    ROUTER_NAME=`echo $i|sed -e "s/.*\/\(.*\)$/\1/g"`
    RES=`sed -e 's/Broadcast: .*$//g' -e '/Destination: 128\/2/d' $i|grep -o "[0-9]\+\.[0-9]\+\.[0-9]\+\.[0-9]\+"|sort|uniq|sed -e ':a N; s/\n/,/g; ta'`
    if [ "x$RES" != "x" ]; then
       echo "${ROUTER_NAME},$RES"
    fi
done 

for i in `find vanilla/junos-out-int -type f`; do
    ROUTER_NAME=`echo $i|sed -e "s/.*\/\(.*\)$/\1/g"`
    RES=`sed -e 's/Broadcast: .*$//g' -e '/Destination: 128\/2/d' $i|grep -o "Local: [0-9]\+\.[0-9]\+\.[0-9]\+\.[0-9]\+"|sort|uniq|sed -e 's/Local: //g'|sed -e ':a N; s/\n/,/g; ta'`
    if [ "x$RES" != "x" ]; then
       echo "${ROUTER_NAME},$RES"
    fi
done 

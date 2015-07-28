#!/bin/bash

DATA_DIR=$1
SCRIPTS_DIR=`dirname $0`

for i in `ls -1 $1`; do
    FN=$DATA_DIR/$i
    grep -c -m1 "Uptime" $FN 2>&1 >/dev/null
    if [ $? -eq 0 ]; then
        sed "/[0-9]\+\.[0-9]\+\.[0-9]\+\.[0-9]\+/p ; d" $FN|awk -f ${SCRIPTS_DIR}/foundry-new-to-csv.awk > $i
    else
	grep -c -m1 "NetMask" $FN 2>&1 >/dev/null
	if [ $? -eq 0 ]; then
	    sed "/[0-9]\+\.[0-9]\+\.[0-9]\+\.[0-9]\+/p ; d" $FN|awk -f ${SCRIPTS_DIR}/foundry-with-netmask-to-csv.awk|python ${SCRIPTS_DIR}/foundry-with-mask-cleanup.py > $i
	else
            sed "/[0-9]\+\.[0-9]\+\.[0-9]\+\.[0-9]\+/p ; d" $FN|awk -f ${SCRIPTS_DIR}/foundry-old-to-csv.awk > $i
	fi
    fi
done

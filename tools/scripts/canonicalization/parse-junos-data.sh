#!/bin/bash

DATA_DIR=$1
SCRIPTS_DIR=`dirname $0`

for i in `ls -1 $1`; do
    FN=$DATA_DIR/$i
    sed -f ${SCRIPTS_DIR}/junos-to-csv.sed $FN |python ${SCRIPTS_DIR}/junos-cleanup.py|awk -F, ' {printf "%s,%s,%s,%s\n", $1,$3,$4,$2; }' > $i
done


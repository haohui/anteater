#!/bin/bash

DATA_DIR=$1
SCRIPTS_DIR=`dirname $0`

for i in `ls -1 $1`; do
    FN=$DATA_DIR/$i
    sed -e '1,/^Gateway/ { /^Gateway/!{ d } }' -e "/^[ ]*--/d" $FN|sed -e ':a; /    [0-9]\+.[0-9]\+.[0-9]\+.[0-9]\+\(\/[0-9]\+\)\? $/N; s/\n           //; ta' |python ${SCRIPTS_DIR}/cisco-to-csv.py |python ${SCRIPTS_DIR}/cisco-cleanup.py > $i
done

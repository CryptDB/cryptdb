#!/bin/sh

tshark -r "$1" -R mysql -T fields \
       -e ip.src -e ip.dst \
       -e tcp.srcport -e tcp.dstport \
       -e mysql.schema -e mysql.query


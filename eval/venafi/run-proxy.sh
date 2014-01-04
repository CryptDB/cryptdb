#!/bin/sh -x

export CRYPTDB_MODE=single
export ENC_BY_DEFAULT=false
export EDBDIR=/home/nickolai/proj/cryptdb
export CRYPTDB_SHADOW=/tmp/shadow-db
exec mysql-proxy --plugins=proxy --event-threads=4 --max-open-files=1024 \
                 --proxy-lua-script=$EDBDIR/mysqlproxy/wrapper.lua \
                 --proxy-address=127.0.0.1:3307 \
                 --proxy-backend-addresses=localhost:3306


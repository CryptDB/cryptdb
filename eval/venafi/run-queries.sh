#!/bin/sh -x

mysql -h 127.0.0.1 -P 3307 Director < ./CreateDB.sql 
./sql-replay.py director61.log


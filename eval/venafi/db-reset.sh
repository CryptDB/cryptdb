#!/bin/sh -x

( cd ../.. && make -j )
sudo stop mysql
( cd ../../ && sudo make install )
sudo start mysql
rm -r /tmp/shadow-db
mkdir /tmp/shadow-db
mysqladmin -f drop Director
mysqladmin create Director


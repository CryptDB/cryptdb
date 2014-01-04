How to Run Mysql Proxy
----------------------

to start proxy:

  % export EDBDIR=<...>/cryptdb
  % mysql-proxy --plugins=proxy \
                --event-threads=4 \
		--max-open-files=1024 \
		--proxy-lua-script=$EDBDIR/mysqlproxy/wrapper.lua \
		--proxy-address=localhost:3307 \
		--proxy-backend-addresses=localhost:3306

to specify username / password / shadow dir for proxy, run before starting proxy:

  % export CRYPTDB_USER=...
  % export CRYPTDB_PASS=...
  % export CRYPTDB_SHADOW=...

to send a single command to mysql:

  % mysql -u root -pletmein -h 127.0.0.1 -P 3307 -e 'command'


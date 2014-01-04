rm -rf $EDBDIR/shadow/*
mysql -u root -pletmein -e "drop database if exists remote_db"
mysql -u root -pletmein -e "drop database if exists cryptdbtest"
mysql -u root -pletmein -e "create database cryptdbtest"
mysql -u root -pletmein -e "drop database if exists cryptdbtest_control"
mysql -u root -pletmein -e "create database cryptdbtest_control"

import sys
import os
import time
import re
import mechanize

flags = "-v"
output_file = "out.html"

ip = "18.26.5.16"
port = "3307"
edbdir = "/home/cat/cryptdb/src/edb"

def prepare():
    db = "mysql -u root -pletmein -e 'DROP DATABASE cryptdb_phpbb; CREATE DATABASE cryptdb_phpbb'"
    os.system(db)
    os.putenv("CRYPTDB_USER","root")
    os.putenv("CRYPTDB_PASS","letmein")
    os.putenv("CRYPTDB_DB","cryptdb_phpbb")
    #uncomment next two lines for verbosity
    #os.putenv("CRYPTDB_LOG","11111111111111111111111111111111")
    os.putenv("CRYPTDB_PROXY_DEBUG","true")
    #comment out next line for verbosity
    os.putenv("CRYPTDB_LOG","00000000000000000000000000000000")
    os.putenv("CRYPTDB_MODE","multi")
    os.putenv("EDBDIR",edbdir)
    os.system("rm $EDBDIR/../apps/phpBB3/config.php")
    os.system("touch $EDBDIR/../apps/phpBB3/config.php")
    os.system("chmod 666 $EDBDIR/../apps/phpBB3/config.php")
    os.system("mv $EDBDIR/../apps/phpBB3/install2 $EDBDIR/../apps/phpBB3/install")
    os.system("cp $EDBDIR/../apps/phpBB3/install/schemas/mysql_will_build_annot.sql $EDBDIR/../apps/phpBB3/install/schemas/mysql_41_schema.sql")
    #uncomment below line for plain text
    #os.system("cp $EDBDIR/../apps/phpBB3/install/schemas/mysql_unannoted.sql $EDBDIR/../apps/phpBB3/install/schemas/mysql_41_schema.sql")

def proxy():
    pid = os.fork()
    if pid == 0:
        os.system("mysql-proxy --plugins=proxy --max-open-files=1024 --proxy-lua-script=$EDBDIR/../mysqlproxy/wrapper.lua --proxy-address=18.26.5.16:3307 --proxy-backend-addresses=18.26.5.16:3306")
        print(":)")
    elif pid < 0:
        print("failed to fork")
    else:
        time.sleep(1)
        db = "mysql -u root -pletmein -h " + ip + " -P " + port  +  " cryptdb_phpbb -e 'DROP FUNCTION IF EXISTS groupaccess; CREATE FUNCTION groupaccess (auth_option_id mediumint(8), auth_role_id mediumint(8)) RETURNS bool RETURN ((auth_option_id = 14) OR (auth_role_id IN (1, 2, 4, 6, 10, 11, 12, 13, 14, 15, 17, 22, 23, 24)));' "
        os.system(db)
        install_phpBB()
        print "Done! You can run tests now!"


def install_phpBB():
    print "installing phpBB..."
    #create forum
    url = "http://" + ip + "/phpBB3/"
    install_url = url + "install/index.php?mode=install&sub="
    br = mechanize.Browser()
    post = "dbms=mysqli&dbhost=" + ip  +  "&dbport=" + port +  "&dbname=cryptdb_phpbb&dbuser=root&dbpasswd=letmein&table_prefix=phpbb_&admin_name=admin&admin_pass1=letmein&admin_pass2=letmein&board_email=cat_red@mit.edu&board_email2=cat_red@mit.edu"
    config = mechanize.urlopen(install_url+"config_file", data=post);
    br.set_response(config)
    post += "&email_enable=1&smtp_delivery=0&smtp_host=&smtp_auth=PLAIN&smtp_user=&smtp_pass=&cookie_secure=0&force_server_vars=0&server_protocol=http://&server_name=18.26.5.16&server_port=80&script_path=/phpBB"
    advanced = mechanize.urlopen(install_url+"advanced", data=post);
    br.set_response(advanced)
    br.select_form(nr=0)
    br.submit()
    br.select_form(nr=0)
    br.submit()

    os.system("mv $EDBDIR/../apps/phpBB3/install $EDBDIR/../apps/phpBB3/install2")

    print "logging in..."
    #login
    br.open(url+"ucp.php?mode=login")
    br.select_form(nr=1)
    br["username"] = "admin"
    br["password"] = "letmein"
    br.submit()
    print "to ACP..."
    #authenticate to go to ACP
    br.follow_link(text="Administration Control Panel")
    br.select_form(nr=1)
    i = str(br.form).find("password")
    j = str(br.form).find("=)",i)
    br[str(br.form)[i:j]] = "letmein"
    br.submit()
    print "getting permissions page..."
    #navigate to group permissions
    br.follow_link(text="Permissions")
    br.follow_link(text="Groups\xe2\x80\x99 permissions")
    #select Newly Registered Users
    br.select_form(nr=0)
    br["group_id[]"] = ["7"]
    br.submit()
    #set all permissions to yes
    print "setting permissions..."
    br.select_form(nr=1)
    i = 1
    while i > 0:
        start = str(br.form).find("setting[7][0][",i)
        if (start < 0):
            break
        end = str(br.form).find("=[",start)
        if (end < 0):
            break
        br[str(br.form)[start:end]] = ["1"]
        i = end
    br.submit()



prepare()
proxy()

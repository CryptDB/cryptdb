#!/bin/bash 
#
# CryptDB onion tool installer

#   Carlos Carvalho
#
# See INSTALL for instructions

apache2="/etc/apache2/sites-enabled/000-default"
declare -a files=(./index.php ./common.php ./select.php ./favicon.ico menu_assets)

function do_install() {

    # be faithful
    if [ $UID != 0 ] ; then
        echo "Root?"
        exit
    fi

    docroot=$1
    type=$2
    currdir=$(pwd)

    ps ax |grep apache2 >/dev/null 2>&1
    if [ $? -ne 0 ] ; then
        echo "* warning: apache2 is not running"
    else
        echo "apache2 running on pid(s): `pidof apache2`"
    fi
    php=$(ls /etc/apache2/mods-enabled/php*.load |cut -d "." -f1)
    if [ $? -ne 0 ] ; then
        echo "* warning: php disabled"
    else
        echo "`basename $php` is enabled"
    fi
    
    # Installing...
    total=$(ls -l $docroot |head -1| awk '{print $2}')
    if [ $total -gt 0 ] ; then

        # Even after backup, keep all data there, but files with
        # the same name will be overwritten.
        echo -n "[!] DocumentRoot directory $docroot is not empty. Backup data? [Y/n]: "
        read opt

        if [ "$opt" != "n" ] ; then
            cd `dirname $docroot`
            currtime=$(date +"%m-%d-%Y_%H:%M")
            tar -vcjf $currdir/www-bkp-$currtime.tar.bz2 `basename $docroot`
            cd - >/dev/null
        fi
    fi

    a=0
    while : ; do 
        if [ -z "${files[$a]}" ] ; then
            break;
        fi

        if [ ! -z $type ] && [ $type == "devel" ] ; then
            sufix=$(echo ${files[$a]} |cut -d "." -f3)
            if [ $sufix == "php" ] ; then
                cd $docroot/
                rm -rf $docroot/${files[$a]}
                ln -s $currdir/${files[$a]} .
                cd - >/dev/null
            else
                rm -rf $docroot/${files[$a]}
                cp -Rpv ${files[$a]} $docroot/
            fi
        else
            rm -rf $docroot/${files[$a]}
            cp -Rpv ${files[$a]} $docroot/
        fi
        a=$(expr $a + 1)
    done
    echo "Done."
}

function install
{
    if [ ! -f $apache2 ] ; then
        echo "Install script defined to use apache2"
        echo "Directory $apache2 not found."
        exit
    fi

    docroot=$(grep DocumentRoot $apache2 |awk '{print $2}')

    if [ -z $docroot ] ; then
        echo "DocumentRoot not found"
        exit
    fi

    type=$1
    do_install $docroot $type
}

#do it
install $1
#EOF


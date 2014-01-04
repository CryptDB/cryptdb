<?php
/*
    Copyright 2009 Sean Proctor

    This file is part of PHP-Calendar.

    PHP-Calendar is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    PHP-Calendar is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PHP-Calendar; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

define('IN_PHPC', true);

$phpc_root_path = './';

require_once($phpc_root_path . "includes/calendar.php");
require_once('adodb/adodb.inc.php');

$have_config = false;
if(file_exists($phpc_root_path . 'config.php')) {
        require_once($phpc_root_path . 'config.php');
        $have_config = true;
} elseif(file_exists($phpc_root_path . 'config.inc.php')) {
        require_once($phpc_root_path . 'config.inc.php');
        $have_config = true;
} elseif(!empty($_GET['configfile'])) {
        if(file_exists($_GET['configfile'])) {
                require_once($_GET['configfile']);
                $have_config = true;
        } else {
                echo "<p>File not found</p>\n";
        }
} else {
        echo "<p>No config file found.</p>\n";
}

if(!$have_config) {
        echo "<form action=\"update.php\" method=\"get\">\n";
        echo "Config file (include full path): ";
        echo "<input name=\"configfile\" type=\"text\" />\n";
        echo "<input type=\"submit\" value=\"Update\" />\n";
        echo "</form>";
        exit;
}

// grab the DB info from the config file
if(defined('SQL_HOST')) {
        $sql_host = SQL_HOST;
        echo "<p>Your host is: $sql_host</p>";
} else {
        soft_error('No hostname found in your config file');
}

if(defined('SQL_USER')) {
        $sql_user = SQL_USER;
        echo "<p>Your SQL username is: $sql_user</p>";
} else {
        soft_error('No username found in your config file');
}

if(defined('SQL_PASSWD')) {
        $sql_passwd = SQL_PASSWD;
        echo "<p>Your SQL password is: $sql_passwd</p>";
} else {
        soft_error('No password found in your config file');
}

if(defined('SQL_DATABASE')) {
        $sql_database = SQL_DATABASE;
        echo "<p>Your SQL database name is: $sql_database</p>";
} else {
        soft_error('No database found in your config file');
}

if(defined('SQL_PREFIX')) {
        $sql_prefix = SQL_PREFIX;
        echo "<p>Your SQL table prefix is: $sql_prefix</p>";
} else {
        soft_error('No table prefix found in your config file');
}

if(defined('SQL_TYPE')) {
        $sql_type = SQL_TYPE;
} elseif(isset($dbms)) {
        $sql_type = $dbms;
} else {
        soft_error('No database type found in your config file');
}
echo "<p>Your database type is: $sql_type</p>";

// connect to the database
$db = NewADOConnection($sql_type);
$ok = $db->Connect($sql_host, $sql_user, $sql_passwd, $sql_database);
if(!$ok) {
        soft_error('Could not connect to the database');
}

update_calno('calendars');
update_calno('events');
update_calno('users');

echo "<p>Paste this into your config.php:</p>";
echo "<pre>" . htmlspecialchars(create_config($sql_host, $sql_user, $sql_passwd,
                        $sql_database, $sql_prefix, $sql_type)) . "</pre>";

function update_calno($table_suffix)
{
        global $db, $calendarName;
        // update calno column in $table_suffix
        $query = "SELECT * FROM " . SQL_PREFIX . "$table_suffix";
        $result = $db->SelectLimit($query, 1)
                or db_error('Error in query', $query);
        if($result->FieldCount() == 0) {
                soft_error("You cannot upgrade a DB with no events.");
        }
        $event = $result->FetchRow();
        if(array_key_exists('calendar', $event)) {
                $calendarName = $event['calendar'];
        } else {
                if(array_key_exists('calno', $event)) {
                        $query = "ALTER TABLE ".SQL_PREFIX."$table_suffix\n"
                                ."CHANGE calno calendar varchar(32)";
                } else {
                        $query = "ALTER TABLE ".SQL_PREFIX."$table_suffix\n"
                                ."ADD calendar varchar(32)";
                }
                $db->Execute($query) or db_error("Error in alter", $query);
                echo "<p>Updated calendar in ".SQL_PREFIX
                        ."$table_suffix table</p>";
        }
}

?>

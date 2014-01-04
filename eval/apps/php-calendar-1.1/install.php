<?php
/*
   Copyright 2007 Sean Proctor

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

/*
   Run this file to install the calendar
   it needs very much work
*/

$phpc_root_path = './';
$calendar_name = '0';

define('IN_PHPC', true);

require_once($phpc_root_path . 'includes/calendar.php');
require_once('adodb/adodb.inc.php');

echo '<html>
<head>
<title>install php calendar</title>
</head>
<body>
<form method="post" action="install.php">
';

foreach($_POST as $key => $value) {
	echo "<input name=\"$key\" value=\"$value\" type=\"hidden\">\n";
}

if(!isset($_POST['config'])) {
	get_config();
} elseif(!isset($_POST['my_hostname'])
		&& !isset($_POST['my_username'])
		&& !isset($_POST['my_passwd'])
		&& !isset($_POST['my_prefix'])
		&& !isset($_POST['my_database'])) {
	get_server_setup();
} elseif((isset($_POST['create_user']) || isset($_POST['create_db']))
		&& !isset($_POST['done_user_db'])) {
	add_sql_user_db();
} elseif(!isset($_POST['base'])) {
	install_base();
} elseif(!isset($_POST['admin_user'])
		&& !isset($_POST['admin_pass'])) {
	get_admin();
} else {
	add_calendar();
}

function get_config()
{
	global $phpc_root_path;

        $config_file = $phpc_root_path . 'config.php';

	if( $file = @fopen($config_file, 'w')) {
                fclose($file);
		echo '<input type="hidden" name="config" value="1">
			<p>your config file is writable</p>
			<input type="submit" value="continue">';
	} else {
		echo '<p>I could not write your configuration file. This file probably does not yet exist. If that is the case then I need to create it, but I could not. You need to give me permission to write to this file. I suggest logging in with a shell and typing:</p>
			<p><code>
			touch config.php<br>
			chmod 666 config.php
			</code></p>
			<p>or if you only have ftp access, upload a blank file named config.php then use the chmod command to change the permissions of config.php to 666.</p>
			<input type="submit" value="Retry">';
	}
}

function get_server_setup()
{
	echo '
		<table class="display">
		<tr>
		<td>SQL server hostname:</td>
		<td><input type="text" name="my_hostname" value="localhost"></td>
		</tr>
		<tr>
		<td>SQL Database name:</td>
		<td><input type="text" name="my_database" value="calendar"></td>
		</tr>
		<tr>
		<td>Table prefix:</td>
		<td><input type="text" name="my_prefix" value="phpc_"></td>
		</tr>
		<tr>
		<td>Username:</td>
		<td><input type="text" name="my_username" value="calendar"></td>
		</tr>
		<tr>
		<td>Password:</td>
		<td><input type="password" name="my_passwd"></td>
		</tr>
		<tr>
		<td>Database type:</td>
		<td><select name="sql_type">
		<option value="mysql">MySQL</option>
		<option value="postgres">PostgreSQL</option>
                <option value="postgres64">PostgreSQL 6.4 or earlier</option>
                <option value="sqlitepo">SQLite (experimental)</option>
		</select>
		</td>
		</tr>
		<tr>
		<td colspan="2">
		  <input type="checkbox" name="create_db" value="yes">
		  create the database (don\'t check this if it already exists)
		</td>
		</tr>
		<tr><td colspan="2">
		  <input type="checkbox" name="create_user" value="yes">
		  Should the user info supplied above be created? Do not check
		  this if the user already exists
		</td></tr>
		<tr><td colspan="2">
		  You only need to provide the following information if you
		  need to create a user
		</td></tr>
		<tr>
		<td>Admin name:</td>
		<td><input type="text" name="my_adminname"></td>
		</tr>
		<tr>
		<td>Amind Password:</td>
		<td><input type="password" name="my_adminpassword"></td>
		</tr>
		<tr>
		<td colspan="2">
		  <input name="action" type="submit" value="Install">
		</td>
		</tr>
		</table>';
}

function add_sql_user_db()
{
	global $db;

	$my_hostname = $_POST['my_hostname'];
	$my_username = $_POST['my_username'];
	$my_passwd = $_POST['my_passwd'];
	$my_prefix = $_POST['my_prefix'];
	$my_database = $_POST['my_database'];
	$my_adminname = $_POST['my_adminname'];
	$my_adminpasswd = $_POST['my_adminpassword'];
	$sql_type = $_POST['sql_type'];

	$create_user = isset($_POST['create_user'])
		&& $_POST['create_user'] == 'yes';
	$create_db = isset($_POST['create_db']) && $_POST['create_db'] == 'yes';

	$db = NewADOConnection($sql_type);

	$string = "";

	if($create_user) {
		$ok = $db->Connect($my_hostname, $my_adminname, $my_adminpasswd,
				'');
	} else {
		$ok = $db->Connect($my_hostname, $my_username, $my_passwd, '');
	}

	if(!$ok) {
		db_error(_('Could not connect to server.'));
	}

	if($create_db) {
		$sql = "CREATE DATABASE $my_database";

		$db->Execute($sql)
			or db_error(_('error creating db'), $sql);

		$string .= "<div>Successfully created database</div>";
	}

	if($create_user) {
		switch($sql_type) {
			case 'mysql':
				$sql = "GRANT ALL ON accounts.* TO $my_username@$my_hostname identified by '$my_passwd'";
				$db->Execute($sql)
					or db_error(_('Could not grant:'), $sql);
				$sql = "GRANT ALL ON $my_database.* to $my_username identified by '$my_passwd'";
				$db->Execute($sql)
					or db_error(_('Could not grant:'), $sql);

				$sql = "FLUSH PRIVILEGES";
				$db->Execute($sql)
					or db_error("Could not flush privileges", $sql);

				$string .= "<div>Successfully added user</div>";

				break;

			default:
				die('we don\'t support creating users for this database type yet');

		}
	}

	echo "$string\n"
		."<div><input type=\"submit\" name=\"done_user_db\" value=\"continue\">"
		."</div>\n";

}

function create_config($sql_hostname, $sql_username, $sql_passwd, $sql_database,
                $sql_prefix, $sql_type)
{
	return "<?php\n"
		."define('SQL_HOST',     '$sql_hostname');\n"
		."define('SQL_USER',     '$sql_username');\n"
		."define('SQL_PASSWD',   '$sql_passwd');\n"
		."define('SQL_DATABASE', '$sql_database');\n"
		."define('SQL_PREFIX',   '$sql_prefix');\n"
		."define('SQL_TYPE',     '$sql_type');\n"
		."?>\n";
}

function install_base()
{
	global $phpc_root_path, $db;

	$sql_type = $_POST['sql_type'];
	$my_hostname = $_POST['my_hostname'];
	$my_username = $_POST['my_username'];
	$my_passwd = $_POST['my_passwd'];
	$my_prefix = $_POST['my_prefix'];
	$my_database = $_POST['my_database'];

	$fp = fopen($phpc_root_path . 'config.php', 'w')
		or soft_error('Couldn\'t open config file.');

	fwrite($fp, create_config($my_hostname, $my_username, $my_passwd,
                                $my_database, $my_prefix, $sql_type))
		or soft_error("could not write to file");
	fclose($fp);

	require_once($phpc_root_path . 'config.php');

        // Make the database connection.
        $db = NewADOConnection(SQL_TYPE);
        if(!$db->Connect(SQL_HOST, SQL_USER, SQL_PASSWD, SQL_DATABASE)) {
                db_error(_("Could not connect to the database"));
        }

	create_tables();

	echo "<p>calendars base created</p>\n"
		."<div><input type=\"submit\" name=\"base\" value=\"continue\">"
		."</div>\n";
}

function create_tables()
{
	global $db;

        $dict = NewDataDictionary($db);

        $flds = "\n" .
		"id I NOTNULL PRIMARY,\n" .
		"uid I,\n" .
		"startdate D,\n" .
		"enddate D,\n" .
		"starttime T,\n" .
		"duration I,\n" .
		"eventtype I,\n" .
		"subject C(255),\n" .
		"description B,\n" .
		"calendar C(32)\n";

        $sqlarray = $dict->CreateTableSQL(SQL_PREFIX . 'events', $flds)
		or soft_error("Problem creating table SQL");
	echo "<pre>"; print_r($sqlarray); echo "</pre>";
	
        $result = $dict->ExecuteSQLArray($sqlarray)
		or soft_error("Problem executing SQL for table.");
	if($result == 1) {
                db_error("Error creating table " . SQL_PREFIX . "events:");
	}
	

	$flds = "\n" .
		"calendar C(32) NOTNULL DEFAULT '0',\n" .
		"uid I NOTNULL PRIMARY,\n" .
		"username C(32) NOTNULL,\n" .
		"password C(32) NOTNULL default '',\n" .
		"admin I1 NOTNULL DEFAULT 0\n";

	$sqlarray = $dict->CreateTableSQL(SQL_PREFIX . 'users', $flds);
	echo "<pre>"; print_r($sqlarray); echo "</pre>";
	
	$result = $dict->ExecuteSQLArray($sqlarray)
		or soft_error("Error creating table " . SQL_PREFIX . "users");
	if($result == 1) {
                db_error("Error creating table " . SQL_PREFIX . "users");
	}

	$flds = "\n" .
		"calendar C(32) NOTNULL PRIMARY,\n" .
		"hours_24 I1 NOTNULL DEFAULT 0,\n" .
		"start_monday I1 NOTNULL DEFAULT 0,\n" .
		"translate I1 NOTNULL DEFAULT 0,\n" .
		"anon_permission I1 NOTNULL DEFAULT 0,\n" .
		"subject_max I2 NOTNULL DEFAULT 32,\n" .
		"contact_name C(255) NOTNULL DEFAULT '',\n" .
		"contact_email C(255) NOTNULL DEFAULT '',\n" .
		"calendar_title C(255) NOTNULL DEFAULT '',\n" .
		"url C(200) NOTNULL DEFAULT ''\n";

	$sqlarray = $dict->CreateTableSQL(SQL_PREFIX . 'calendars', $flds);
	echo "<pre>"; print_r($sqlarray); echo "</pre>";
	$result = $dict->ExecuteSQLArray($sqlarray)
		or soft_error("Error creating table " . SQL_PREFIX
				. "calendars");
	if($result == 1) {
                db_error("Error creating table " . SQL_PREFIX . "calendars");
	}
}

function get_admin()
{

	echo "<table>\n"
		."<tr><td colspan=\"2\">The following is to log in to the "
		."calendar (not the SQL admin)</td></tr>\n"
		."<tr><td>\n"
		."Admin name:\n"
		."</td><td>\n"
		."<input type=\"text\" name=\"admin_user\" />\n"
		."</td></tr><tr><td>\n"
		."Admin password:"
		."</td><td>\n"
		."<input type=\"password\" name=\"admin_pass\" />\n"
		."</td></tr><tr><td colspan=\"2\">"
		."<input type=\"submit\" value=\"Create Admin\" />\n"
		."</td></tr></table>\n";

}

function add_calendar()
{
	global $db, $phpc_root_path, $calendar_name;

	require_once($phpc_root_path . 'config.php');

	$hours_24 = 0;
	$start_monday = 0;
	$translate = 1;
	$calendar_title = 'PHP-Calendar';

        // Make the database connection.
        $db = NewADOConnection(SQL_TYPE);
        if(!$db->Connect(SQL_HOST, SQL_USER, SQL_PASSWD, SQL_DATABASE)) {
                db_error(_("Could not connect to the database"));
        }

	$query = "INSERT INTO ".SQL_PREFIX."calendars (calendar, hours_24, "
	."start_monday, translate, subject_max, calendar_title) "
	."VALUES ('$calendar_name', '$hours_24', '$start_monday', '$translate',"
	." '32', '$calendar_title')";

	$result = $db->Execute($query);

	if(!$result) {
		db_error("Couldn't create calendar.", $query);
	}

	echo "<p>calendar created</p>\n";

	$query = "insert into ".SQL_PREFIX."users\n"
		."(uid, username, password, calendar) VALUES\n"
		."('".$db->GenID(SQL_PREFIX.'uid', 0)."', 'anonymous', '', "
                ."$calendar_name)";

	$result = $db->Execute($query);
	if(!$result) {
		db_error("Could not add anonymous user:", $query);
	}

	$passwd = md5($_POST['admin_pass']);

	$query = "insert into ".SQL_PREFIX."users\n"
		."(uid, username, password, calendar, admin) VALUES\n"
		."('".$db->GenID(SQL_PREFIX.'uid')."', '$_POST[admin_user]', "
                ."'$passwd', $calendar_name, 1)";

	$result = $db->Execute($query);
	if(!$result) {
		db_error("Could not add admin:", $query);
	}
	
	echo "<p>admin added; <a href=\"index.php\">View calendar</a></p>";
	echo '<p>You should now delete install.php and chmod 444 config.php</p>';
}

echo '</form></body></html>';
?>

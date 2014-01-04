<?php
/*
   Copyright 2002 - 2005 Sean Proctor, Nathan Poiro

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
   This file sets up the global variables to be used later
*/

// Modify these if you need to
$phpc_script = $_SERVER['PHP_SELF'];
if($_SERVER["HTTPS"] == "on") $phpc_protocol = "https";
else $phpc_protocol = "http";
$phpc_server = $_SERVER['SERVER_NAME'];
$phpc_url = "$phpc_protocol://$phpc_server$phpc_script?"
	. $_SERVER['QUERY_STRING'];

/* FIXME: This file is a fucking mess, clean it up */

if(!defined('IN_PHPC')) {
       die("Hacking attempt");
}

ini_set('arg_separator.output', '&amp;');

// Run the installer if we have no config file
if(!file_exists($phpc_root_path . 'config.php')) {
        redirect('install.php');
        exit;
}
require_once($phpc_root_path . 'config.php');
if(!defined('SQL_TYPE')) {
        redirect('install.php');
        exit;
}

require_once($phpc_root_path . 'includes/calendar.php');
require_once('adodb/adodb.inc.php');

// Make the database connection.
$db = NewADOConnection(SQL_TYPE);
if(!$db->Connect(SQL_HOST, SQL_USER, SQL_PASSWD, SQL_DATABASE)) {
        db_error(_("Could not connect to the database"));
}

session_start();

// Create vars
foreach($_GET as $key => $value) {
	if(!get_magic_quotes_gpc())
		$vars[$key] = addslashes($value);
	else
		$vars[$key] = $value;
}

foreach($_POST as $key => $value) {
	if(!get_magic_quotes_gpc())
		$vars[$key] = addslashes($value);
	else
		$vars[$key] = $value;
}

// Load configuration
if(!empty($vars['calendar_name'])) {
        $calendar_name = $vars['calendar_name'];
} // calendar name is otherwise set to 0

$query = "SELECT * from " . SQL_PREFIX .
        "calendars WHERE calendar='$calendar_name'";

$result = $db->SelectLimit($query, 1)
        or db_error(_('Could not read configuration') . ": $query");

$config = $result->FetchRow($result);

if($config['start_monday'])
	$first_day_of_week = 1;
else
	$first_day_of_week = 0;

require_once($phpc_root_path . 'includes/globals.php');

// set day/month/year
if (!isset($vars['month'])) {
	$month = date('n');
} else {
	$month = $vars['month'];
}

if(!isset($vars['year'])) {
	$year = date('Y');
} else {
        if($vars['year'] > 2037) {
                soft_error(_('That year is too far in the future')
                        . ": {$vars['year']}");
        } elseif($vars['year'] < 1970) {
                soft_error(_('That year is too far in the past')
                        . ": {$vars['year']}");
        }
	$year = date('Y', mktime(0, 0, 0, $month, 1, $vars['year']));
}

if(!isset($vars['day'])) {
	if($month == date('n') && $year == date('Y')) {
                $day = date('j');
	} else {
                $day = 1;
        }
} else {
	$day = ($vars['day'] - 1) % date('t', mktime(0, 0, 0, $month, 1, $year))
                + 1;
}

while($month < 1) $month += 12;
$month = ($month - 1) % 12 + 1;

//set action
if(empty($vars['action'])) {
	$action = 'display';
} else {
	$action = $vars['action'];
}

// setup translation stuff
if($config['translate'] && empty($no_gettext)) {

	if(isset($vars['lang']) && in_array($vars['lang'], $languages)) {
		$lang = $vars['lang'];
		setcookie('lang', $vars['lang']);
	} elseif(isset($_COOKIE['lang'])) {
		$lang = $_COOKIE['lang'];
	} elseif(isset($_SERVER['HTTP_ACCEPT_LANGUAGE']) && in_array(
				substr($_SERVER['HTTP_ACCEPT_LANGUAGE'], 0, 2),
				$languages)) {
		$lang = substr($_SERVER['HTTP_ACCEPT_LANGUAGE'], 0, 2);
	} else {
		$lang = 'en';
	}

	switch($lang) {
		case 'de':
			setlocale(LC_ALL, 'de_DE@euro', 'de_DE', 'de', 'ge');
			break;
		case 'en':
			setlocale(LC_ALL, 'en_US', 'C');
			break;
                case 'es':
                        setlocale(LC_ALL, 'es_ES@euro', 'es_ES', 'es');
			break;
                case 'it':
                        setlocale(LC_ALL, 'it_IT@euro', 'it_IT', 'it');
			break;
                case 'ja':
                        setlocale(LC_ALL, 'ja_JP', 'ja');
                        break;
                case 'nl':
                        setlocale(LC_ALL, 'nl_NL@euro', 'nl_NL', 'nl');
                        break;
	}

	bindtextdomain('messages', $phpc_root_path . 'locale');
	textdomain('messages');
} else {
	$lang = "en";
}
?>

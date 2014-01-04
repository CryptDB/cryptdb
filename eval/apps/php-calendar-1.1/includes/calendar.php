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
   this file contains all the re-usable functions for the calendar
*/

if ( !defined('IN_PHPC') ) {
       die("Hacking attempt");
}

include($phpc_root_path . 'includes/html.php');
include($phpc_root_path . 'includes/url_match.php');
$urlmatch = new urlmatch;

// make sure that we have _ defined
if(!function_exists('_')) {
	function _($str) { return $str; }
	$no_gettext = 1;
}

// called when some error happens
function soft_error($str)
{
	echo '<html><head><title>'._('Error')."</title></head>\n"
		.'<body><h1>'._('Software Error')."</h1>\n"
                ."<h2>"._('Message:')."</h2>\n"
		."<pre>$str</pre>\n";
        if(version_compare(phpversion(), '4.3.0', '>=')) {
                echo "<h2>"._('Backtrace')."</h2>\n";
                echo "<ol>\n";
                foreach(debug_backtrace() as $bt) {
                        echo "<li>$bt[file]:$bt[line] - $bt[function]</li>\n";
                }
                echo "</ol>\n";
        }
        echo "</body></html>\n";
	exit;
}

// called when there is an error involving the DB
function db_error($str, $query = "")
{
        global $db;

        $string = "$str<br />".$db->ErrorNo().': '.$db->ErrorMsg();
        if($query != "") {
                $string .= "<br />"._('SQL query').": $query";
        }
        soft_error($string);
}

// takes a number of the month, returns the name
function month_name($month)
{
        global $month_names;

	$month = ($month - 1) % 12 + 1;
        return $month_names[$month];
}

//takes a day number of the week, returns a name (0 for the beginning)
function day_name($day)
{
	global $day_names;

	$day = $day % 7;

        return $day_names[$day];
}

function short_month_name($month)
{
        global $short_month_names;

	$month = ($month - 1) % 12 + 1;
        return $short_month_names[$month];
}

// checks global variables to see if the user is logged in.
function is_user()
{
	global $calendar_name;

	return isset($_SESSION["uid$calendar_name"]);
}

// returns whether or not the current user can access stuff by $name
function check_user($uid)
{
	global $calendar_name;

	return isset($_SESSION["uid$calendar_name"])
		&& ($_SESSION["uid$calendar_name"] == $uid || is_admin());
}

function is_admin()
{
	global $calendar_name;

	return !empty($_SESSION["admin$calendar_name"]);
}

function login_user($user, $password)
{
        global $calendar_name, $db;

        $passwd = md5($password);

	$query= "SELECT uid, admin FROM ".SQL_PREFIX."users\n"
		."WHERE username='$user' "
                ."AND password='$passwd' "
		."AND calendar='$calendar_name'";

	$result = $db->Execute($query)
                or db_error("error checking user", $query);

	$row = $result->FetchRow();

        if(empty($row))
                return false;

	$_SESSION["uid$calendar_name"] = $row['uid'];
	if(!empty($row['admin']))
		$_SESSION["admin$calendar_name"] = true;

	session_write_close();

	return true;
}

// takes a time string, and formats it according to type
// returns the formatted string
function formatted_time_string($time, $type)
{
	global $config;

	switch($type) {
		default:
			preg_match('/(\d+):(\d+)/', $time, $matches);
			$hour = $matches[1];
			$minute = $matches[2];

			if(!$config['hours_24']) {
				if($hour >= 12) {
                                        if($hour != 12) {
                                                $hour -= 12;
                                        }
					$pm = ' PM';
                                } else {
                                        if($hour == 0) {
                                                $hour = 12;
                                        }
					$pm = ' AM';
				}
			} else {
				$pm = '';
			}

			return sprintf('%d:%02d%s', $hour, $minute, $pm);
		case 2:
			return _('FULL DAY');
		case 3:
			return _('TBA');
                case 4:
                        return '';
	}
}

// takes start and end dates and returns a nice display
function formatted_date_string($startyear, $startmonth, $startday, $endyear,
		$endmonth, $endday)
{
	$str = month_name($startmonth) . " $startday, $startyear";

	if($startyear != $endyear || $startmonth != $endmonth || $startday !=
			$endday) {
		$str .= " - " . month_name($endmonth) . " $endday, $endyear";
	}
	return $str;
}

// returns tag data for a link for $lang
function lang_link($lang)
{
	global $phpc_script;

        $str = $_SERVER['QUERY_STRING'];
        $str = preg_replace("/&lang=\\w*/", '', $str);
        $str = preg_replace("/lang=\\w*&/", '', $str);
        $str = preg_replace("/lang=\\w*/", '', $str);
	if(!empty($str)) {
		$str = htmlentities($str) . '&amp;';
	}
	$str = "{$phpc_script}?{$str}lang=$lang";

	return tag('a', attributes("href=\"$str\""), $lang);
}

// returns tag data for the links at the bottom of the calendar
function link_bar()
{
	global $config, $phpc_url, $phpc_root_path, $languages;

	$html = tag('div', attributes('class="phpc-footer"'));

	if($config['translate']) {
		$lang_links = tag('p', '[', lang_link('en'), '] ');
                foreach($languages as $lang) {
                        if(file_exists("$phpc_root_path/locale/$lang/LC_MESSAGES/messages.mo")) {
                                $lang_links->add('[', lang_link($lang), '] ');
                        }
                }
                $html->add($lang_links);
	}

	$html->add(tag('p', '[',
			tag('a',
				attributes('href="http://validator.w3.org/'
					.'check?url='
					.rawurlencode($phpc_url)
					.'"'), 'Valid HTML 4.01 Strict'),
			'] [',
			tag('a', attributes('href="http://jigsaw.w3.org/'
					.'css-validator/check/referer"'),
					'Valid CSS2'),
			']'));
	return $html;
}

// returns all the events for a particular day
function get_events_by_date($day, $month, $year)
{
	global $calendar_name, $db;

/* event types:
1 - Normal event
2 - full day event
3 - to be announced
4 - no time
5 - weekly event
6 - monthly event
7 - yearly event
*/
        $startdate = $db->SQLDate('Y-m-d', 'startdate');
        $enddate = $db->SQLDate('Y-m-d', 'enddate');
        $date = $db->DBDate(date('Y-m-d', mktime(0, 0, 0, $month, $day, $year)));

        // day of week
        $dow_startdate = $db->SQLDate('w', 'startdate');
        $dow_date = $db->SQLDate('w', $date);
        // day of month
        $dom_startdate = $db->SQLDate('d', 'startdate');
        $dom_date = $db->SQLDate('d', $date);
	// month of year
	$moy_startdate = $db->SQLDate('m', 'startdate');
	$moy_date = $db->SQLDate('m', $date);

	$query = 'SELECT * FROM '.SQL_PREFIX."events\n"
		."WHERE $date >= $startdate AND $date <= $enddate\n"
                // find normal events
                ."AND (eventtype = 1 OR eventtype = 2 OR eventtype = 3 "
                ."OR eventtype = 4\n"
                // find weekly events
		."OR (eventtype = 5 AND $dow_startdate = $dow_date)\n"
                // find monthly events
		."OR (eventtype = 6 AND $dom_startdate = $dom_date)\n"
                // find yearly events
		."OR (eventtype = 7 AND $dom_startdate = $dom_date AND $moy_startdate = $moy_date)\n"
                .")\n"
                // in the current calendar
		."AND calendar = '$calendar_name'\n"
		."ORDER BY starttime";

	$result = $db->Execute($query)
		or db_error(_('Error in get_events_by_date'), $query);

	return $result;
}

// returns the event that corresponds to $id
function get_event_by_id($id)
{
	global $calendar_name, $db;

	$events_table = SQL_PREFIX . 'events';
	$users_table = SQL_PREFIX . 'users';

	$query = "SELECT $events_table.*,\n"
		.$db->SQLDate('Y', "$events_table.startdate")." AS year,\n"
		.$db->SQLDate('m', "$events_table.startdate")." AS month,\n"
		.$db->SQLDate('d', "$events_table.startdate")." AS day,\n"
		.$db->SQLDate('Y', "$events_table.enddate")." AS end_year,\n"
		.$db->SQLDate('m', "$events_table.enddate")." AS end_month,\n"
		.$db->SQLDate('d', "$events_table.enddate")." AS end_day,\n"
		."$users_table.username\n"
		."FROM $events_table\n"
		."LEFT JOIN $users_table\n"
		."ON ($events_table.uid = $users_table.uid)\n"
		."WHERE $events_table.id = '$id'\n"
		."AND $events_table.calendar = '$calendar_name';";

	$result = $db->Execute($query);

	if(!$result) {
		db_error(_('Error in get_event_by_id'), $query);
	}

	if($result->FieldCount() == 0) {
		soft_error("item doesn't exist!");
	}

	return $result->FetchRow();
}

// parses a description and adds the appropriate mark-up
function parse_desc($text)
{
	global $urlmatch;

	// get out the crap, put in breaks
        $text = strip_tags($text);
	// if you want to allow some tags, change the previous line to:
	// $text = strip_tags($text, "a"); // change "a" to the list of tags
        $text = htmlspecialchars($text, ENT_NOQUOTES);
	// then uncomment the following line
	// $text = preg_replace("/&lt;(.+?)&gt;/", "<$1>", $text);
        $text = nl2br($text);

	//urls
	$text = $urlmatch->match($text);

	// emails
	$text = preg_replace("/([a-zA-Z][\w\.-]*[a-zA-Z0-9]@[a-zA-Z0-9][\w\.-]*"
			."[a-zA-Z0-9]\.[a-zA-Z][a-zA-Z\.]*[a-zA-Z])/",
			"<a href=\"mailto:$1\">$1</a>", $text );

	return $text;
}

// returns the day of week number corresponding to 1st of $month
function day_of_first($month, $year)
{
	return date('w', mktime(0, 0, 0, $month, 1, $year));
}

// returns the number of days in $month
function days_in_month($month, $year)
{
	return date('t', mktime(0, 0, 0, $month, 1, $year));
}

//returns the number of weeks in $month
function weeks_in_month($month, $year)
{
	global $first_day_of_week;

	return ceil((days_in_month($month, $year) + (7 + day_of_first($month, $year) - $first_day_of_week) % 7) / 7);
}

// creates a link with text $text and GET attributes corresponding to the rest
// of the arguments.
// returns tag data for the link
function create_id_link($text, $action, $id = false, $attribs = false)
{
	global $phpc_script;

	$url = "href=\"$phpc_script?action=$action";
	if($id !== false) $url .= "&amp;id=$id";
	$url .= '"';

        if($attribs !== false) {
                $as = attributes($url, $attribs);
        } else {
                $as = attributes($url);
        }
	return tag('a', $as, $text);
}

function create_date_link($text, $action, $year = false, $month = false,
                $day = false, $attribs = false, $lastaction = false)
{
        global $phpc_script;

	$url = "href=\"$phpc_script?action=$action";
	if($year !== false) $url .= "&amp;year=$year";
	if($month !== false) $url .= "&amp;month=$month";
	if($day !== false) $url .= "&amp;day=$day";
        if($lastaction !== false) $url .= "&amp;lastaction=$lastaction";
	$url .= '"';

        if($attribs !== false) {
                $as = attributes($url, $attribs);
        } else {
                $as = attributes($url);
        }
	return tag('a', $as, $text);
}

// takes a menu $html and appends an entry
function menu_item_append(&$html, $name, $action, $year = false, $month = false,
		$day = false, $lastaction = false)
{
        if(!is_object($html)) {
                soft_error('Html is not a valid Html class.');
        }
	$html->add(create_date_link($name, $action, $year, $month,
                                        $day, false, $lastaction));
        $html->add("\n");
}

// same as above, but prepends the entry
function menu_item_prepend(&$html, $name, $action, $year = false,
		$month = false, $day = false, $lastaction = false)
{
        $html->prepend("\n");
	$html->prepend(create_date_link($name, $action, $year, $month,
                                $day, false, $lastaction));
}

// creates a hidden input for a form
// returns tag data for the input
function create_hidden($name, $value)
{
	return tag('input', attributes("name=\"$name\"", "value=\"$value\"",
				'type="hidden"'));
}

// creates a submit button for a form
// return tag data for the button
function create_submit($value)
{
	return tag('input', attributes('name="submit"', "value=\"$value\"",
                                'type="submit"'));
}

// creates a text entry for a form
// returns tag data for the entry
function create_text($name, $value = false)
{
	$attributes = attributes("name=\"$name\"", 'type="text"');
	if($value !== false) {
		$attributes->add("value=\"$value\"");
	}
	return tag('input', $attributes);
}

// creates a password entry for a form
// returns tag data for the entry
function create_password($name)
{
	return tag('input', attributes("name=\"$name\"", 'type="password"'));
}

// creates a checkbox for a form
// returns tag data for the checkbox
function create_checkbox($name, $value, $checked = false)
{
	$attributes = attributes("name=\"$name\"", 'type="checkbox"',
			"value=\"$value\"");
	if(!empty($checked)) $attributes->add('checked="checked"');
	return tag('input', $attributes);
}

function can_add_event()
{
        global $config;

        return $config['anon_permission'] || is_user();
}

// creates the navbar for the top of the calendar
// returns tag data for the navbar
function navbar()
{
	global $vars, $action, $config, $year, $month, $day;

	$html = tag('div', attributes('class="phpc-navbar"'));

	if(can_add_event() && $action != 'add') { 
		menu_item_append($html, _('Add Event'), 'event_form', $year,
				$month, $day);
	}

	if($action != 'search') {
		menu_item_append($html, _('Search'), 'search', $year, $month,
				$day);
	}

	if(!empty($vars['day']) || !empty($vars['id']) || $action != 'display') {
		menu_item_append($html, _('Back to Calendar'), 'display',
				$year, $month);
	}

	if($action != 'display' || !empty($vars['id'])) {
		menu_item_append($html, _('View date'), 'display', $year,
				$month, $day);
	}

	if(is_user()) {
		menu_item_append($html, _('Log out'), 'logout',
                                empty($vars['year']) ? false : $year,
                                empty($vars['month']) ? false : $month,
				empty($vars['day']) ? false : $day,
				$action);
	} else {
		menu_item_append($html, _('Log in'), 'login',
                                empty($vars['year']) ? false : $year,
                                empty($vars['month']) ? false : $month,
				empty($vars['day']) ? false : $day,
                                $action);
	}

	if(is_admin() && $action != 'admin') {
		menu_item_append($html, _('Admin'), 'admin');
	}

	if(isset($var['display']) && $var['display'] == 'day') {
		$monthname = month_name($month);

		$lasttime = mktime(0, 0, 0, $month, $day - 1, $year);
		$lastday = date('j', $lasttime);
		$lastmonth = date('n', $lasttime);
		$lastyear = date('Y', $lasttime);
		$lastmonthname = month_name($lastmonth);

		$nexttime = mktime(0, 0, 0, $month, $day + 1, $year);
		$nextday = date('j', $nexttime);
		$nextmonth = date('n', $nexttime);
		$nextyear = date('Y', $nexttime);
		$nextmonthname = month_name($nextmonth);

		menu_item_prepend($html, "$lastmonthname $lastday",
					'display', $lastyear, $lastmonth,
					$lastday);
		menu_item_append($html, "$nextmonthname $nextday",
				'display', $nextyear, $nextmonth, $nextday);
	}

	return $html;
}

// creates an array from $start to $end, with an $interval
function create_sequence($start, $end, $interval = 1, $display = NULL)
{
        $arr = array();
        for ($i = $start; $i <= $end; $i += $interval){
                if($display) {
                        $arr[$i] = call_user_func($display, $i);
                } else {
                        $arr[$i] = $i;
                }
        }
        return $arr;
}

function minute_pad($minute)
{
        return sprintf('%02d', $minute);
}

function redirect($page) {
	global $phpc_script, $phpc_server, $phpc_protocol;

	if($page{0} == "/") {
		$dir = '';
	} else {
		$dir = dirname($phpc_script) . "/";
	}

	header("Location: $phpc_protocol://$phpc_server$dir$page");
}

?>

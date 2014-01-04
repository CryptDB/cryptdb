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

if(!defined('IN_PHPC')) {
       die("Hacking attempt");
}

function logout()
{
	global $calendar_name, $vars, $day, $month, $year, $phpc_script;

        unset($_SESSION["uid$calendar_name"]);
	unset($_SESSION["admin$calendar_name"]);
        session_write_close();

	$string = "$phpc_script?";
        $arguments = array();
/* We might not have permission for the last action. We can probably
 * assume that the user is going away.
 * TODO: add a logout page here with a redirect to the front page of the
 *       calendar they were just viewing

        if(!empty($vars['lastaction']))
                $arguments[] = "action=$vars[lastaction]";
 */

        if(!empty($vars['year']))
                $arguments[] = "year=$year";
        if(!empty($vars['month']))
                $arguments[] = "month=$month";
        if(!empty($vars['day']))
                $arguments[] = "day=$day";
        redirect($string . implode('&', $arguments));

        return tag('h2', _('Loggin out...'));
}
?>

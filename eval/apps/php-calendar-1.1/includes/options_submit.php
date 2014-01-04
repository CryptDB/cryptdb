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

if ( !defined('IN_PHPC') ) {
       die("Hacking attempt");
}

function options_submit()
{
	global $calendar_name, $vars, $db;

        if(!is_admin()) {
                return tag('div', _('Permission denied'));
        }

        $query = "UPDATE ".SQL_PREFIX."calendars SET\n";

        if(isset($vars['hours_24']))
                $query .= "hours_24 = 1,\n";
        else
                $query .= "hours_24 = 0,\n";

        if(isset($vars['start_monday']))
                $query .= "start_monday = 1,\n";
        else
                $query .= "start_monday = 0,\n";

        if(isset($vars['translate']))
                $query .= "translate = 1,\n";
        else
                $query .= "translate = 0,\n";

        $query .= "anon_permission = '$vars[anon_permission]',\n"
                ."calendar_title = '$vars[calendar_title]',\n"
                ."subject_max = '$vars[subject_max]'\n"
                ."WHERE calendar=$calendar_name;";

        $result = $db->Execute($query)
                or db_error(_('Error reading options'), $query);

        return tag('div', _('Updated options'));
}

?>

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

function new_user_submit()
{
	global $calendar_name, $vars, $db;

        if(!is_admin()) {
                return tag('div', _('Permission denied'));
        }

        if(empty($vars['user_name'])) {
                return tag('div', _('You must specify a user name'));
        }

        if(empty($vars['password1'])) {
                return tag('div', _('You must specify a password'));
        }

        if(empty($vars['password2'])
                || $vars['password1'] != $vars['password2']) {
                return tag('div', _('Your passwords did not match'));
        }

	if(empty($vars['make_admin'])) {
		$make_admin = '0';
	} else {
		$make_admin = '1';
	}

        $passwd = md5($vars['password1']);

        $query = "SELECT uid FROM ".SQL_PREFIX."users\n"
                ."WHERE username='$vars[user_name]'";

        $result = $db->Execute($query)
                or db_error(_('Unexpected error while checking if user exists.'),
				$query);

        if($result->RecordCount() > 0) {
                $row = $result->FetchRow();
                $query = "UPDATE ".SQL_PREFIX."users\n"
                        ."SET password='$passwd',\n"
			."admin=$make_admin\n"
                        ."WHERE uid={$row['uid']}";

		$text = _('Modified user.');
        } else {
                /* start the UIDs at 2 to be backward compatible with all
                   calendars that don't have a uid sequence */
                $query = "INSERT into ".SQL_PREFIX."users\n"
                        ."(uid, username, password, calendar, admin) VALUES\n"
                        ."('".$db->GenID(SQL_PREFIX . 'uid', 2)."', "
                        ."'$vars[user_name]', '$passwd', '$calendar_name', "
			."$make_admin)";

		$text = _('Added user.');
        }

        $result = $db->Execute($query)
                or db_error(_('Error creating user.'), $query);

        return tag('div', $text);
}

?>

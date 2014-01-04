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

function admin()
{
        if(!is_admin()) {
                soft_error(_('You must be logged in as an admin.'));
        }

	return tag('div', options_form(), new_user_form());
}

define('CHECK', 1);
define('TEXT', 2);
define('DROPDOWN', 3);

// name, text, type, value(s)
$config_form = array(
        array('start_monday', _('Start Monday'), CHECK),
        array('hours_24', _('24 Hour Time'), CHECK),
        array('translate', _('Translate'), CHECK),
        array('calendar_title', _('Calendar Title'), TEXT),
        array('subject_max', _('Maximum Subject Length'), TEXT),
        array('anon_permission', _('Anonymous Users'), DROPDOWN,
                array(_('Cannot add nor modify events'),
                        _('Can add but not modify events'),
                        _('Can add and modify events'))));

function options_form()
{
	global $config, $phpc_script, $config_form;

        $tbody = tag('tbody');

        foreach($config_form as $element) {
                $name = $element[0];
                $text = $element[1];
                $type = $element[2];

                switch($type) {
                        case CHECK:
                                $input = create_checkbox($name, '1',
                                                $config[$name]);
                                break;
                        case TEXT:
                                $input = create_text($name, $config[$name]);
                                break;
                        case DROPDOWN:
                                $sequence = create_sequence(0,
                                                count($element[3]) - 1);
                                $input = create_select($name, $element[3],
                                                $config[$name], $sequence);
                                break;
                        default:
                                soft_error(_('Unsupported config type')
                                                . ": $type");
                }

                $tbody->add(tag('tr',
                                tag('th', $text.':'),
                                tag('td', $input)));
        }

        return tag('form', attributes("action=\"$phpc_script\"",
                                'method="post"'),
			tag('table', attributes('class="phpc-main"'),
				tag('caption', _('Options')),
				tag('tfoot',
                                        tag('tr',
                                                tag('td', attributes('colspan="2"'),
							create_hidden('action', 'options_submit'),
							create_submit(_('Submit'))))),
				$tbody));

}

function new_user_form()
{
	global $phpc_script;

	return tag('form', attributes("action=\"$phpc_script\"",
                                'method="post"'),
			tag('table', attributes('class="phpc-main"'),
				tag('caption', _('Create/Modify User')),
				tag('tfoot',
					tag('tr',
						tag('td', attributes('colspan="2"'),
							create_hidden('action', 'new_user_submit'),
							create_submit(_('Submit'))))),
				tag('tbody',
					tag('tr',
						tag('th', _('User Name').':'),
						tag('td', create_text('user_name'))),
					tag('tr',
						tag('th', _('Password').':'),
						tag('td', create_password('password1'))),
					tag('tr',
						tag('th', _('Confirm Password').':'),
						tag('td', create_password('password2'))),
					tag('tr',
						tag('th', _('Make Admin').':'),
						tag('td', create_checkbox('make_admin', '1')))
				   )));
}

?>

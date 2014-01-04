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

function event_form()
{
	global $vars, $day, $month, $year, $db, $config, $phpc_script,
               $month_names, $event_types;

	if(isset($vars['id'])) {
		// modifying
		$id = $vars['id'];

		$title = sprintf(_('Editing Event #%d'), $id);

		$row = get_event_by_id($id);

		$subject = htmlspecialchars(stripslashes($row['subject']));
		$desc = htmlspecialchars(stripslashes($row['description']));

		$year = $row['year'];
		$month = $row['month'];
		$day = $row['day'];

                $hour = date('H', strtotime($row['starttime']));
		$minute = date('i', strtotime($row['starttime']));

		$end_year = $row['end_year'];
		$end_month = $row['end_month'];
		$end_day = $row['end_day'];

		$durmin = $row['duration'] % 60;
		$durhr  = floor($row['duration'] / 60);

		if(!$config['hours_24']) {
			if($hour > 12) {
				$pm = true;
				$hour = $hour - 12;
			} elseif($hour == 12) {
                                $pm = true;
                        } else {
                                $pm = false;
                        }
                }

		$typeofevent = $row['eventtype'];
		$multiday = $year != $end_year || $month != $end_month
			|| $day != $end_day;

	} else {
		// case "add":
		$title = _('Adding event to calendar');

		$subject = '';
                $desc = '';
                if($day == date('j') && $month == date('n')
                                && $year == date('Y')) {
                        if($config['hours_24']) {
                                $hour = date('G');
                        } else {
                                $hour = date('g');
                                if(date('a') == 'pm') {
                                        $pm = true;
                                } else {
                                        $pm = false;
                                }
                        }
                } else {
                        $hour = 6;
                        $pm = true;
                }

                $minute = 0;
		$end_day = $day;
		$end_month = $month;
		$end_year = $year;
		$durhr = 1;
		$durmin = 0;
		$typeofevent = 1;
		$multiday = false;
	}

        if($config['hours_24']) {
                $hour_sequence = create_sequence(0, 23);
        } else {
                $hour_sequence = create_sequence(1, 12);
        }
        $minute_sequence = create_sequence(0, 59, 5, 'minute_pad');
        $year_sequence = create_sequence(1970, 2037);

	$html_time = tag('td',
			create_select('hour', $hour_sequence, $hour),
			tag('b', ':'),
			create_select('minute', $minute_sequence, $minute));

	if(!$config['hours_24']) {
		if($pm) {
                        $value = 1;
                } else {
                        $value = 0;
                }
		$html_time->add(create_select('pm', array(_('AM'), _('PM')),
                                        $value));
	}

	if(isset($id)) $input = create_hidden('id', $id);
	else $input = '';

	$attributes = attributes('class="phpc-main"');

        $day_of_month_sequence = create_sequence(1, 31);

	return tag('form', attributes("action=\"$phpc_script\""),
			tag('table', $attributes,
				tag('caption', $title),
				tag('tfoot',
					tag('tr',
						tag('td', attributes( 'colspan="2"'),
							$input,
							create_submit(_("Submit Event")),
							create_hidden('action', 'event_submit')))),
				tag('tbody',
					tag('tr',
						tag('th', _('Date of event')),
						tag('td',
							create_select('day', $day_of_month_sequence, $day),
							create_select('month', $month_names, $month),
							create_select('year', $year_sequence, $year))),
					tag('tr',
						tag('th', _('Multiple day event')),
						tag('td', create_checkbox('multiday', '1', $multiday))),
					tag('tr',
						tag('th', _('End date (Multiple day events only)')),
						tag('td',
							create_select('endday', $day_of_month_sequence, $end_day),
							create_select('endmonth', $month_names, $end_month),
							create_select('endyear', $year_sequence, $end_year))),
					tag('tr',
						tag('th', _('Event type')),
						tag('td',
							create_select('typeofevent',
								$event_types, $typeofevent))),
					tag('tr',
						tag('th',  _('Time')),
						$html_time),
					tag('tr',
						tag('th', _('Duration')),
						tag('td',
							create_select('durationhour', create_sequence(0, 23), $durhr),
							_('hour(s)') . "\n",
							create_select('durationmin', $minute_sequence, $durmin),
							_('minutes') . "\n")),
					tag('tr',
						tag('th', _('Subject').' ('.$config['subject_max'].' '._('chars max').')'),
						tag('td', tag('input', attributes('type="text"', "size=\"{$config['subject_max']}\"", "maxlength=\"{$config['subject_max']}\"", 'name="subject"', "value=\"$subject\"")))),
					tag('tr',
						tag('th',  _('Description')),
						tag('td',
							tag('textarea', attributes('rows="5"',
									'cols="50"',
									'name="description"'),
								$desc))))));
}

?>

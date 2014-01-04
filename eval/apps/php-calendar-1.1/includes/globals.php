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

if ( !defined('IN_PHPC') ) {
       die("Hacking attempt");
}

$languages = array('de', 'en', 'es', 'it', 'ja', 'nl');

$month_names = array(
                1 => _('January'),
                _('February'),
                _('March'),
                _('April'),
                _('May'),
                _('June'),
                _('July'),
                _('August'),
                _('September'),
                _('October'),
                _('November'),
                _('December'),
                );

$day_names = array(
                _('Sunday'),
		_('Monday'),
		_('Tuesday'),
		_('Wednesday'),
		_('Thursday'),
		_('Friday'),
		_('Saturday'),
                );

$short_month_names = array(
		1 => _('Jan'),
		_('Feb'),
		_('Mar'),
		_('Apr'),
		_('May'),
		_('Jun'),
		_('Jul'),
		_('Aug'),
		_('Sep'),
		_('Oct'),
		_('Nov'),
		_('Dec'),
                );

$event_types = array(
                1 => _('Normal'),
                _('Full Day'),
                _('To Be Announced'),
                _('No Time'),
                _('Weekly'),
                _('Monthly'),
		_('Yearly'),
                );

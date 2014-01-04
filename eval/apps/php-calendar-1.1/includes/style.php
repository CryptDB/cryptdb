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
   This file is loaded as a style sheet
*/

header('Content-Type: text/css');

if(isset($_GET['bgcolor1'])) {
	$bgcolor1 = $_GET['bgcolor'];
} else {
	$bgcolor1 = BG_COLOR1;
}
/*
   FIXME: you get the idea, eventually the colors should be pickable by a user,
   but we need a real concept of users first
 */
$bgcolor2 = BG_COLOR2;
$bgcolor3 = BG_COLOR3;
$bgcolor4 = BG_COLOR4;
$bgpast = BG_PAST;
$bgfuture = BG_FUTURE;
$sepcolor = SEPCOLOR;
$textcolor1 = TEXTCOLOR1;
$textcolor2 = TEXTCOLOR2;
?>
body {
  font-family: "Times New Roman", serif;
  margin: 0 2%;
  padding: 0;
  background-color: <?php echo $bgcolor1 ?>;
  color: <?php echo $textcolor1 ?>;
}

a {
  color: <?php echo $textcolor1 ?>;
   background-color: inherit;
}

a:hover {
  color: <?php echo $bgcolor2 ?>;
  background-color: inherit;
}

h1 {
  font-size: 200%;
  text-align: center;
  font-family: sans-serif;
  color: <?php echo $textcolor1 ?>;
  background-color: inherit;
}

h2 {
  font-size: 175%;
  text-align: center;
  font-family: serif;
  color: <?php echo $textcolor1 ?>;
  background-color: inherit;
}

input[type="submit"] {
  background-color: <?php echo $bgcolor3 ?>;
  color: <?php echo $textcolor1 ?>;
  border: 1px solid <?php echo $sepcolor ?>;
}

.phpc-navbar a {
  background-color: <?php echo $bgcolor3 ?>;
  color: <?php echo $textcolor1 ?>;
  border: 1px solid <?php echo $sepcolor ?>;
}

input[type="submit"]:hover {
  background-color: <?php echo $textcolor1 ?>;
  color: <?php echo $bgcolor3 ?>;
}

.phpc-navbar a:hover {
  background-color: <?php echo $textcolor1 ?>;
  color: <?php echo $bgcolor3 ?>;
}

.phpc-navbar {
  margin: 1em 0 2em 0;
  text-align: center;
}

.phpc-navbar a {
  font-size: 90%;
  text-decoration: none;
  margin: 0;
  padding: 2px;
}

.phpc-main {
  font-size: 90%;
  border-style: solid;
  border-collapse: collapse;
  border-color: <?php echo $sepcolor ?>;
  border-width: 2px;
  color: <?php echo $textcolor1 ?>;
  background-color: <?php echo $bgcolor4 ?>;
}

table.phpc-main {
  width: 100%;
}

.phpc-main h2 {
  margin: 0;
  text-align: left;
  background-color: <?php echo $bgcolor3 ?>;
  padding: .25em; 
  border-color: <?php echo $sepcolor ?>;
  border-style: solid;
  border-width: 0 0 2px 0;
}

.phpc-main div {
  margin: .5em;
  font-weight: bold;
}

.phpc-main p {
  border-style: solid;
  border-width: 2px 0 0 0;
  border-color: <?php echo $sepcolor ?>;
  padding: .5em;
  margin: 0;
  text-align: justify;
}

caption {
  font-size: 175%;
  color: <?php echo $textcolor1 ?>;
  background-color: <?php echo $bgcolor1 ?>;
  padding: 2px;
  font-weight: bolder;
}

thead th {
  background-color: <?php echo $bgcolor3 ?>;
  color: <?php echo $textcolor1 ?>;
}

thead {
  border: 1px solid <?php echo $sepcolor ?>;
}

thead, tfoot {
  text-align: center;
}

#calendar td, #calendar th {
  border-style: solid;
  border-collapse: collapse;
  border-color: <?php echo $sepcolor ?>;
  border-width: 2px;
  padding: .5em;
}

table.phpc-main tbody th {
  text-align: right;
}

#calendar {
  table-layout: fixed;
}

#calendar td {
  text-align: left;
  height: 80px;
  overflow: hidden;
}

td.past {
  background-color: <?php echo $bgpast ?>;
  color: inherit;
}

td.future {
  background-color: <?php echo $bgfuture ?>;
  color: inherit;
}

td.none {
  background-color: <?php echo $bgcolor2 ?>;
  color: inherit;
}

table.phpc-main ul {
  margin: 2px;
  padding: 0;
  list-style-type: none;
  border-color: <?php echo $sepcolor ?>;
  border-style: solid;
  border-width: 1px 1px 0 1px;
}

table.phpc-main li {
  font-size: 80%;
  font-weight: normal;
  padding: 0;
  border-color: <?php echo $sepcolor ?>;
  border-style: solid;
  border-width: 0 0 1px 0;
  margin: 0;
}

table.phpc-main li a {
  display: block;
  text-decoration: none;
  padding: 2px;
}

table.phpc-main li a:hover {
  background-color: <?php echo $bgcolor2 ?>;
  color: <?php echo $textcolor2 ?>;
}

.phpc-list {
  border: 1px solid <?php echo $sepcolor ?>;
}

.phpc-footer {
  text-align: center;
}

.phpc-button {
  text-align: center;
}

.phpc-add {
  float: right;
  text-align: right;
}

/* \*/ /*/
  #calendar {table-layout: auto;}
/* */


<?php
/*
   Copyright 2009 Sean Proctor

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

$HtmlInline = array('a', 'strong');

/*
 * data structure to display XHTML
 * see function tag() below for usage
 * Cateat: this class does not understand HTML, it just approximates it.
 *	do not give an empty tag that needs to be closed without content, it
 *	won't be close. ex tag('div')
 */
class Html {
        var $tagName;
        var $attributeList;
        var $childElements;
	var $error_func;

        function Html() {
                $args = func_get_args();
                return call_user_func_array(array(&$this, '__construct'),
                                $args);
        }

        function __construct() {
		$error_func = array(&$this, 'default_error_handler');
                $args = func_get_args();
                $this->tagName = array_shift($args);
                if($this->tagName === NULL) $this->tagName = '';
                $this->attributeList = array();
                $this->childElements = array();

                $arg = array_shift($args);
                if($arg === NULL) return;

                if(is_a($arg, 'AttributeList')) {
                        $this->attributeList = $arg;
                        $arg = array_shift($args);
                }

                while($arg !== NULL) {
                        $this->add($arg);
                        $arg = array_shift($args);
                }
        }

        function add() {
                $htmlElements = func_get_args();
                foreach($htmlElements as $htmlElement) {
                        if(is_array($htmlElement)) {
                                foreach($htmlElement as $element) {
                                        $this->add($element);
                                }
                        } elseif(is_object($htmlElement)
                                        && !is_a($htmlElement, 'Html')) {
                                html_error('Invalid class: '
                                                . get_class($htmlElement));
                        } else {
                                $this->childElements[] = $htmlElement;
                        }
                }
        }

        function prepend() {
                $htmlElements = func_get_args();
                foreach(array_reverse($htmlElements) as $htmlElement) {
                        if(is_array($htmlElement)) {
                                foreach(array_reverse($htmlElement)
                                                as $element) {
                                        $this->prepend($element);
                                }
                        } elseif(is_object($htmlElement)
                                        && !is_a($htmlElement, 'Html')) {
                                html_error('Invalid class: '
                                                . get_class($htmlElement));
                        } else {
                                array_unshift($this->childElements,
                                                $htmlElement);
                        }
                }
        }

        function toString() {
                global $HtmlInline;

		if($this->tagName != '') {
			$output = "<{$this->tagName}";

			if($this->attributeList != NULL) {
				$output .= ' '
					. $this->attributeList->toString();
			}

			if(sizeof($this->childElements) == 0) {
				$output .= ">\n";
				return $output;
			}

			$output .= ">";
		}

                foreach($this->childElements as $child) {
                        if(is_object($child)) {
                                if(is_a($child, 'Html')) {
                                        $output .= $child->toString();
                                } else {
                                        html_error('Invalid class: '
                                                        . get_class($child));
                                }
                        } else {
                                $output .= $child;
                        }
                }

		if($this->tagName != '') {
			$output .= "</{$this->tagName}>";

			if(!in_array($this->tagName, $HtmlInline)) {
				$output .= "\n";
			}
		}

                return $output;
        }

	function default_error_handler($str) {
		echo "<html><head><title>Error</title></head>\n"
			."<body><h1>Software Error</h1>\n"
			."<h2>Message:</h2>\n"
			."<pre>$str</pre>\n";
		if(version_compare(phpversion(), '4.3.0', '>=')) {
			echo "<h2>Backtrace</h2>\n";
			echo "<ol>\n";
			foreach(debug_backtrace() as $bt) {
				echo "<li>{$bt['file']}:{$bt['line']} - ";
				if(!empty($bt['class']))
					echo "{$bt['class']}{$bt['type']}";
				echo "{$bt['function']}(" . implode(', ',
							$bt['args'])
					. ")</li>\n";
			}
			echo "</ol>\n";
		}
		echo "</body></html>\n";
		exit;
	}

	/* call this function if you want a non-default error handler */
	function html_set_error_handler($func) {
		$this->error_func = $func;
	}

	function html_error() {
		$args = func_get_args();
		return call_user_func_array($this->error_func, $args);
	}
}

/*
 * Data structure to display XML style attributes
 * see function attributes() below for usage
 */
class AttributeList {
        var $list;

        function AttributeList() {
                $args = func_get_args();
                return call_user_func_array(array(&$this, '__construct'),
                                $args);
        }

        function __construct() {
                $this->list = array();
                $args = func_get_args();
                $this->add($args);
        }

        function add() {
                $args = func_get_args();
                foreach($args as $arg) {
                        if(is_array($arg)) {
                                foreach($arg as $attr) {
                                        $this->add($attr);
                                }
                        } else {
                                $this->list[] = $arg;
                        }
                }
        }

        function toString() {
                return implode(' ', $this->list);
        }
}

/*
 * creates an Html data structure
 * arguments are tagName [AttributeList] [Html | array | string] ...
 * where array contains an array, Html, or a string, same requirements for that
 * array
 */
function tag()
{
        $args = func_get_args();
        $html = new Html();
        call_user_func_array(array(&$html, '__construct'), $args);
        return $html;
}

/*
 * creates an AttributeList data structure
 * arguments are [attribute | array] ...
 * where attribute is a string of name="value" and array contains arrays or
 * attributes
 */
function attributes()
{
        $args = func_get_args();
        $attrs = new AttributeList();
        call_user_func_array(array(&$attrs, '__construct'), $args);
        return $attrs;
}


// creates a select element for a form
// returns HTML data for the element
function create_select($name, $arr, $default)
{
	$html = tag('select', attributes('size="1"', "name=\"$name\""));

	foreach($arr as $value => $text) {
		$attributes = attributes("value=\"$value\"");
		if($value == $default) $attributes->add('selected');
		$html->add(tag('option', $attributes, $text));
	}

	return $html;
}

// creates a select element for a form given a certain range
// returns HTML data for the element
function create_select_range($name, $lbound, $ubound, $increment = 1,
		$default = false, $name_function = false)
{
	$arr = array();

	for ($i = $lbound; $i <= $ubound; $i += $increment){
		if($name_function !== false) {
			$text = $name_function($i);
		} else {
			$text = $i;
		}
		$arr[$i] = $text;
	}
	return create_select($name, $arr, $default);
}

?>

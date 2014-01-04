<?php

require("select.php");
 $ACCESS_PWD=''; #!!!IMPORTANT!!! this is script access password, SET IT if you want to protect you DB from public access

 #DEFAULT db connection settings
 # --- WARNING! --- if you set defaults - it's recommended to set $ACCESS_PWD to protect your db!
$DBDEF=array(
    'user'=>"",#required
    'pwd'=>"", #required
    'db'=>"",  #optional, default DB
    'host'=>"",#optional
    'port'=>"",#optional
    'chset'=>"utf8",#optional, default charset
);

$CRYPTDBDEF=array(
    'user'=>"",#required
    'pwd'=>"", #required
    'db'=>"",  #optional, default DB
    'host'=>"",#optional
    'port'=>"",#optional
    'chset'=>"utf8",#optional, default charset
);

$SESSIONSDEF=array(

    's_PROXY'=>"",
    's_DB'=>"",
    's_QUERY'=>"",
    's_ID'=>"",
    's_TABLE'=>"",
);

 date_default_timezone_set('UTC');#required by PHP 5.1+

/*
 * Proxy defaults 
 */
ini_set("mysqli.default_host", "localhost");
ini_set("mysqli.database_name", "cryptdbtest");
ini_set("mysqli.default_port", "3306");
ini_set("mysqli.default_user", "root");

//constants
 $VERSION='1.8.120510';
 $MAX_ROWS_PER_PAGE=50; #max number of rows in select per one page
 $D="\r\n"; #default delimiter for export
 $BOM=chr(239).chr(187).chr(191);
 $SHOW_T="SHOW TABLE STATUS";
 $DB=array(); #working copy for DB settings
 $CRYPTDB=array(); #working copy for CryptDB settings

 $self=$_SERVER['PHP_SELF'];

 session_start();
 if (!isset($_SESSION['XSS'])) $_SESSION['XSS']=get_rand_str(16);
 $xurl='XSS='.$_SESSION['XSS'];

 ini_set('display_errors',1);  #TODO turn off before deploy
 error_reporting(E_ALL ^ E_NOTICE);

//strip quotes if they set
 if (get_magic_quotes_gpc()){
     $_COOKIE=array_map('killmq',$_COOKIE);
     $_REQUEST=array_map('killmq',$_REQUEST);
 }

 if (!$ACCESS_PWD) {
     $_SESSION['is_logged']=true;
     loadcfg();
 }

 if (isset($_REQUEST['login'])){
     if ($_REQUEST['pwd']!=$ACCESS_PWD){
         $err_msg="Invalid password. Try again";
     }else{
         $_SESSION['is_logged']=true;
         loadcfg();
     }
 }

 if (isset($_REQUEST['logoff'])){
     check_xss();
     $_SESSION = array();
     savecfg();
     $url=$self;
     //if (!$ACCESS_PWD) $url='/';
     $_SESSION['logoff']=true;
     header("location: $url");
     exit;
 }

 if (!isset($_SESSION['is_logged'])){
     print_login();
     exit;
 }

 if (isset($_REQUEST['savecfg'])){
     check_xss();
     savecfg();
 }

 loadsess();

 if (isset($_REQUEST['showcfg'])){
     print_cfg();
     exit;
 }

 function curr_q($q, $action)
 {
     static $query = "";
     if(!$action)
         return $query;

     return($query = $q);
 }

 function session_add($id, $val) {
     $_SESSION[$id] = $val;
 }
 
 function session_append($id, $val) {
     $_SESSION[$id] .= $val;
 }
 
 function session_remove($id) {
    $value = $_SESSION[$id];
    unset($_SESSION[$id]);
    return $value;
 }
 
 function session_debug($id) {
     if(isset($_SESSION[$id]))
         echo "SESSION $id:" . $_SESSION[$id] . "<br>";
     else
         echo "SESSION: not found<br>";
 }

// More generic functions
function killmq($value){
    return is_array($value)?array_map('killmq',$value):stripslashes($value);
}

 /*
  * Login helper
  */
function print_login(){
    print_header();
?>
<center>
<h3>Access protected by password</h3>
<div style="width:400px;border:1px solid #999999;background-color:#eeeeee">
Password: <input type="password" name="pwd" value="">
<input type="hidden" name="login" value="1">
<input type="submit" value=" Login ">
</div>
</center>
<?php
    print_footer();
}


/*
 * Debug helper
 */
function rw($s){#for debug
 echo $s."<br>\n";
}

/*
 * Validade XSS
 */
function check_xss(){
    global $self;
    if ($_SESSION['XSS']!=trim($_REQUEST['XSS'])){
        unset($_SESSION['XSS']);
        header("location: $self");
        exit;
    }
}

/*
 * Generates random alpha numeric strngs
 */
function get_rand_str($len){
    $result='';
    $chars=preg_split('//','ABCDEFabcdef0123456789');
    for($i=0;$i<$len;$i++) $result.=$chars[rand(0,count($chars)-1)];
    return $result;
}

/*
 * Recursive , returns string array
 */
function get_close_char($str, $pos, $ochar){
    $aCLOSE=array(
        '\'' => '(?<!\\\\)\'|(\\\\+)\'',
        '"' => '(?<!\\\\)"',
        '/*' => '\*\/',
        '#' => '[\r\n]+',
        '--' => '[\r\n]+',
    );
    if ( $aCLOSE[$ochar] && preg_match("/(".$aCLOSE[$ochar].")/", $str, $m, PREG_OFFSET_CAPTURE, $pos ) ) {
        $clchar=$m[1][0];
        $clpos=$m[1][1];
        $sl=strlen($m[2][0]);
        if ($ochar=="'" && $sl){
            if ($sl % 2){ #don't count as CLOSE char if number of slashes before ' ODD
                list($clchar, $clpos)=get_close_char($str, $clpos+strlen($clchar), $ochar);
            }else{
                $clpos+=strlen($clchar)-1;$clchar="'";#correction
            }
        }
    }
    return array($clchar, $clpos);
}

/*
 * Another string manipulation function
*/
function get_open_char($str, $pos){
    if ( preg_match("/(\/\*|^--|(?<=\s)--|#|'|\"|;)/", $str, $m, PREG_OFFSET_CAPTURE, $pos) ) {
        $ochar=$m[1][0];
        $opos=$m[1][1];
    }
    return array($ochar, $opos);
}

/*
 * Read from insql 
*/
function get_next_chunk($insql, $fname){
    global $LFILE, $insql_done;
    if ($insql) {
        if ($insql_done){
            return '';
        }else{
            $insql_done=1;
            return $insql;
        }
    }
    if (!$fname) return '';
    if (!$LFILE){
        $LFILE=fopen($fname,"r+b") or die("Can't open [$fname] file $!");
    }
    return fread($LFILE, 64*1024);
}

/*
 * Configuration persistence
*/
function savecfg(){
    $v=$_REQUEST['v'];
    $_SESSION['DB']=$v;
    $c=$_REQUEST['c'];
    $_SESSION['CRYPTDB']=$c;
    unset($_SESSION['sql_sd']);

    if ($_REQUEST['rmb']){
        $tm=time()+60*60*24*30;
        setcookie("conn[db]",  $v['db'],$tm);
        setcookie("conn[user]",$v['user'],$tm);
        setcookie("conn[pwd]", $v['pwd'],$tm);
        setcookie("conn[host]",$v['host'],$tm);
        setcookie("conn[port]",$v['port'],$tm);
        setcookie("conn[chset]",$v['chset'],$tm);

        setcookie("cconn[db]",  $c['db'],$tm);
        setcookie("cconn[user]",$c['user'],$tm);
        setcookie("cconn[pwd]", $c['pwd'],$tm);
        setcookie("cconn[host]",$c['host'],$tm);
        setcookie("cconn[port]",$c['port'],$tm);
        setcookie("cconn[chset]",$c['chset'],$tm);
    }else{
        setcookie("conn[db]",  FALSE,-1);
        setcookie("conn[user]",FALSE,-1);
        setcookie("conn[pwd]", FALSE,-1);
        setcookie("conn[host]",FALSE,-1);
        setcookie("conn[port]",FALSE,-1);
        setcookie("conn[chset]",FALSE,-1);


        setcookie("cconn[db]",  FALSE,-1);
        setcookie("cconn[user]",FALSE,-1);
        setcookie("cconn[pwd]", FALSE,-1);
        setcookie("cconn[host]",FALSE,-1);
        setcookie("cconn[port]",FALSE,-1);
        setcookie("cconn[chset]",FALSE,-1);
    }
}

/*
 * Login only
*/
function loadcfg(){
    global $DBDEF, $CRYPTDBDEF;

    if( isset($_COOKIE['conn']) ){
        $a=$_COOKIE['conn'];
        $_SESSION['DB']=$_COOKIE['conn'];
    }else{
        $_SESSION['DB']=$DBDEF;
    }
    if (!strlen($_SESSION['DB']['chset'])) $_SESSION['DB']['chset']=$DBDEF['chset'];#don't allow empty charset


    if( isset($_COOKIE['cconn']) ){
        $a=$_COOKIE['cconn'];
        $_SESSION['CRYPTDB']=$_COOKIE['cconn'];
    }else{
        $_SESSION['CRYPTDB']=$CRYPTDBDEF;
    }
    if (!strlen($_SESSION['CRYPTDB']['chset'])) $_SESSION['CRYPTDB']['chset']=$CRYPTDBDEF['chset'];#don't allow empty charset

}

/*
 * Charset selector
*/
function chset_select($sel=''){
    global $DBDEF;
    $result='';
    if (isset($_SESSION['sql_chset'])){
        $arr=$_SESSION['sql_chset'];
    }else{
        $arr=db_array("show character set",NULL,1);
        if (!is_array($arr)) $arr=array(array('Charset'=>$DBDEF['chset']));
        $_SESSION['sql_chset']=$arr;
    }

    return @sel($arr,'Charset',$sel);
}

//END
?>

<?php


//print_r($_POST);
function do_sql($q){ 
 global $dbh,$last_sth,$last_sql,$reccount,$out_message,$SQLq,$SHOW_T;
 $SQLq=$q;

 if (!do_multi_sql($q)){
     $out_message="Error: ".mysql_error($dbh);
 }else{
     if ($last_sth && $last_sql){
         $SQLq=$last_sql;
         if (preg_match("/^select|show|explain|desc/i",$last_sql)) {
             if ($q!=$last_sql) $out_message="Results of the last select displayed:";
             display_select($last_sth,$last_sql);
         } else {
             $reccount=mysql_affected_rows($dbh);
             $out_message="Done.";
             if (preg_match("/^insert|replace/i",$last_sql)) $out_message.=" Last inserted id=".get_identity();
             if (preg_match("/^drop|truncate/i",$last_sql)) do_sql($SHOW_T);
         }
     }
 }
}

function do_cryptdb_sql($cdbh, $q){
 global $last_sth,$last_sql,$reccount,$out_message,$SQLq,$SHOW_T;
 $SQLq=$q;

 if (!do_multi_sql($q)){
     $out_message="Error: ".$cdbh->error;
 }else{
     if ($last_sth && $last_sql){
         $SQLq=$last_sql;
         if (preg_match("/^select|show|explain|desc/i",$last_sql)) {
             if ($q!=$last_sql) $out_message="Results of the last select displayed:";
             display_select($last_sth,$last_sql);
         } else {
             $reccount=mysql_affected_rows($cryptdbh);
             $out_message="Done.";
             if (preg_match("/^insert|replace/i",$last_sql)) $out_message.=" Last inserted id=".get_identity();
             if (preg_match("/^drop|truncate/i",$last_sql)) do_sql($SHOW_T);
         }
     }
 }
}

function display_select($sth,$q){
    global $dbh,$DB,$sqldr,$reccount,$is_sht,$xurl;
    $rc=array("o","e");
    $dbn=$DB['db'];
    $sqldr='';

    $keys = array_keys($_POST);
    $is_enclevel = strstr($keys[0], "cryptdb");
    if(isset($_POST['cryptdb_describe_table']) || $is_enclevel != FALSE)
    {
        $is_shd=(preg_match('/^show\s+databases/i',$q));
        $is_sht=(preg_match('/^show\s+tables|^SHOW\s+TABLE\s+STATUS/',$q));
        $is_show_crt=(preg_match('/^show\s+create\s+table/i',$q));

        if ($sth===FALSE or $sth===TRUE) return;#check if $sth is not a mysql resource

        $reccount=mysql_num_rows($sth);
        $fields_num=mysql_num_fields($sth);

        $w='';

        $sqldr.="<table class='res $w'>";
        $headers="<tr class='h'>";
        if ($is_sht) $headers.="<td><input type='checkbox' name='cball' value='' onclick='chkall(this)'></td>";
        for($i=0;$i<$fields_num;$i++){
            if ($is_sht && $i>0) break;
            $meta=mysql_fetch_field($sth,$i);
            $headers.="<th>".$meta->name."</th>";
        }
        $headers.="<th>Submit column for analysis</th>";
        $headers.="</tr>\n";
        $sqldr.=$headers;
        $swapper=false;
        $idpos = 0;
        while($row=mysql_fetch_row($sth))
        {

            $identifier = $row[0];

            $sqldr.="<tr class='".$rc[$swp=!$swp]."' onmouseover='tmv(this)' onmouseout='tmo(this)' onclick='tc(this)' align=\"center\">";
            for($i=0;$i<$fields_num;$i++){
                $v=$row[$i];

                $more='';

                if ($is_show_crt) $v="<pre>$v</pre>";
                $sqldr.="<td>$v".(!strlen($v)?"<br>":'')."</td>";

            }
            $sqldr.= "<td><form  action=\"$self\" value=$dbn$idpos  method=\"post\">";
    
            if($is_enclevel == FALSE)
            {
                session_add('s_PROXY', "CryptDBProxy");
                session_add('s_QUERY', $q);
                session_add('s_DB', $DB['db']);
                session_add('s_TABLE', $_POST['cryptdb_describe_table']);
                session_append('s_ID', $identifier . "&");
            }

            $var = $idpos . '_cryptdb_sensitive';
            $sqldr.= "<select name=\"$var\" id=\"$var\" onChange=\"\"> 
                <option name=sensitive_field value=\"sensitive_field\"  selected>Sensitive Field</option> 
                <option name=best_effort_encryption value=\"best_effort_encryption\">Best Effort Encryption</option> 
                <option name=unencrypted value=\"unencrypted\">Unencrypted</option>
                </select><input type=\"submit\" value=\"Submit\" /></td>";
            $sqldr.= "</form></td>";
            $idpos++;
        }
        $sqldr.= "</td></tr></table>\n.$abtn";
    } else {
        //do something
    }
}

function do_one_sql($sql){
    global $last_sth,$last_sql,$MAX_ROWS_PER_PAGE,$page,$is_limited_sql;
    $sql=trim($sql);
    $sql=preg_replace("/;$/","",$sql);
    if ($sql){
        $last_sql=$sql;$is_limited_sql=0;
        if (preg_match("/^select/i",$sql) && !preg_match("/limit +\d+/i", $sql)){
            $offset=$page*$MAX_ROWS_PER_PAGE;
            $sql.=" LIMIT $offset,$MAX_ROWS_PER_PAGE";
            $is_limited_sql=1;
        }
        $last_sth=db_query($sql,0,'noerr');
        return $last_sth;
    }
    return 1;
}

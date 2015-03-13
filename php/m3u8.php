<?php
/**
 * m3u8.php?datetime=2015-03-13_09-00-00 
 * m3u8.php?time=09-00-00 ( assuming localtime )
 * AAAA-MM-DD_HH-MM-SS
 * generate a m3u8 playable starting from given time/datetime
 * useful for vlc
 * http://<myip>/live/m3u8.php?datetime=2015-03-13_07-00-00&id=<camid>
 */

  header("Cache-Control: no-cache, must-revalidate"); 
  header("Expires: Sat, 26 Jul 1997 05:00:00 GMT");

  date_default_timezone_set("Europe/Rome");

  function getParameterValue ($params,$name,$default=null){  
    $value=$default;
    if ($params){
      if (array_key_exists ($name,$params)){
        if ($params[$name]) {
          $value = $params[$name];
        }
      }
    } 
    return $value;
  }

  // parse datetime

  $sname = session_name ();
  $sid = getParameterValue ($_GET,$sname);
  if ($sid){
    session_id($sid);
  }

  session_start();
  $gmt = new DateTimeZone("GMT");

  $ts_req = new DateTime();
  $ts_req->setTimeZone($gmt);

  $ts_at_request = getParameterValue ($_SESSION,'ts_at_request');
  $ts_requested = getParameterValue ($_SESSION,'ts_requested');
  $dt=null;
  $thetime = getParameterValue ($_GET,'time');
  if ( $thetime ) {
    $dt = DateTime::createFromFormat("H-i-s", $thetime);    
  } elseif ($datetime = getParameterValue ($_GET,'datetime')){    
    $dt = DateTime::createFromFormat("Y-m-d_H-i-s", $datetime);
  } elseif ($ts_at_request && $ts_requested){

    // echo "session management\n";
    // print_r ($ts_at_request);
    // print_r ($ts_requested);
    // get directory name 
    //$dname = date_format ($dt,"Y-m-d_H");
    // echo "dname [$dname]";
    // get filename
    //$fname = date_format ($dt,"Y-m-d_H_m_s");
    // echo "fname [$fname]";

    $wcam_id = $_SESSION['wcam_id'];
    // print_r ($ts_req);
    // ts_requested must be added from difference of dates
    $deltas = (int) date_format ($ts_req,"U") - (int) date_format($ts_at_request,"U");

    // TODO: compute sequence ( must match .ini )
    $chunk_size=10;

    $deltas=(int) ($deltas/$chunk_size); $deltas*=$chunk_size;

    // echo "deltasec $deltas .";
    $ts_requested = clone $ts_requested;
    $ts_requested->add (new DateInterval("PT${deltas}S"));
    // print_r ($ts_requested);     

    $sequence = (int) ($deltas / $chunk_size);

    $chunkdelta = new DateInterval ("PT${chunk_size}S");
    header('Content-Type: application/x-mpegURL');
    // generate m3u8
    echo "#EXTM3U\n";
    echo "#EXT-X-VERSION:3\n";
    printf ("#EXT-X-TARGETDURATION:%.3f\n", $chunk_size);
    echo "#EXT-X-MEDIA-SEQUENCE:".$sequence."\n";
    // TODO: load from .ini
    $chunks=3; 
    for ($i=0; $i<$chunks; $i++){
      printf ("#EXTINF:%.3f,\n", $chunk_size);
    // get directory name 
      $dname = date_format ($ts_requested,"Y-m-d_H");
    // echo "dname [$dname]";
    // get filename
      $fname = date_format ($ts_requested,"Y-m-d_H-i-s").".ts";
      $f = sprintf ("%s/chunks/%s/%s", $wcam_id,$dname,$fname );
      echo $f;      
      $ts_requested->add ($chunkdelta);
    }
    die;
  } 

  if ( $dt ) {
    // print_r ($dt); 
    // convert to GMT
    $dt->setTimeZone($gmt);

    // set seconds modulo 10
    $secs = (int)date_format ($dt,"s");
    $secs%=10;
    // echo "seconds $secs\n";
    $dt->sub (new DateInterval("PT${secs}S"));
    // print_r ($dt); 
    $_SESSION['ts_requested'] = $dt;
    $_SESSION['ts_at_request'] = $ts_req;
    $wcam_id = getParameterValue ($_GET,"id","tetto");
    $_SESSION['wcam_id'] = $wcam_id;

    $sname = session_name ();
    $value = session_id ();

    $base = "http://".$_SERVER['HTTP_HOST'].$_SERVER['REQUEST_URI'];    
    $ar = parse_url ($base);

    $redir = sprintf ("%s://%s%s?%s=%s", $ar['scheme'],$ar['host'],$ar['path'],$sname,$value);
    // print_r ($redir);
    header('Location: '.$redir);

    // print_r ($dt);
    // print_r ($ts_req);
  }

?>
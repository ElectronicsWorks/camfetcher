<?php
  header("Cache-Control: no-cache, must-revalidate"); 
  header("Expires: Sat, 26 Jul 1997 05:00:00 GMT");


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
  $datetime = getParameterValue ($_GET,'datetime');
  $thetime = getParameterValue ($_GET,'time');
  $id = getParameterValue ($_GET,'id','tetto');
    // call m3u8.php and fetch redirector

  $base = "http://".$_SERVER['HTTP_HOST'].$_SERVER['REQUEST_URI'];    
  $ar = parse_url ($base);
//  print_r ($ar); die;

  $out = exec ("curl -s -D - \"http://".$ar['host']."/live/m3u8.php?time=${thetime}&datetime=${datetime}&id=${id}\" | grep Location");
  // extract location
  if (preg_match ( "/^Location: (.*)/" , $out,$arr)){
    $url = $arr[1];
  }
  //echo $url; die;
?>
<!doctype html>
<html>
<head>
    
    <title>Flash HLS Live : Flowplayer</title>

    <link rel="shortcut icon" href="http://flash.flowplayer.org/favicon.ico">
    <!-- standalone page styling. can be removed -->
    <style>
        body{
            width:982px;
            margin:50px auto;
            font-family:sans-serif;
        }
        a:active {
            outline:none;
        }
        :focus { -moz-outline-style:none; }

        .palert {
            padding: 12px;
            color: black;
            background-color: #ededed;
            box-shadow: none;
        }
    </style>
    
    <!-- flowplayer javascript component -->
    <script src="http://releases.flowplayer.org/js/flowplayer-3.2.13.min.js"></script>

    </head>

<body>
    <style>
#player {
     display: block;
     width: 640px;
     height: 480px;
     margin: 0 auto;
     cursor: pointer;
}
#flashls_live *:focus {
     outline-style: none;
}
</style>

<script src="http://releases.flowplayer.org/js/flowplayer.ipad-3.2.13.min.js" type="text/javascript"></script>

<!-- container with splash image -->
<div id="player">
    
</div>

<script>

flowplayer("player", 'flowplayer-3.2.18.swf', {
  // Flowplayer configuration options
  // ...
  plugins: {
    flashls: {
      // flashls configuration options
      url: 'flashlsFlowPlayer.swf',
      hls_debug: false,
      hls_debug2: false,
      hls_lowbufferlength: 3,
      hls_minbufferlength: 8,
      hls_maxbufferlength: 30,
      hls_startfromlowestlevel: false,
      hls_seekfromlowestlevel: false,
      hls_live_flushurlcache: false,
      hls_seekmode: 'ACCURATE',
      hls_capleveltostage: false,
      hls_maxlevelcappingmode: 'downscale'
    }
  },
    clip: {
        url: "<?=$url?>",
        live: true,
        
        // configure the flashls plugin as the clip's provider and urlResolver
        provider: "flashls",
        urlResolvers: "flashls",
        autoPlay: true,
        bufferLength:1,
        start:0.1,
        audio:false,

        scaling: "fit"
    }

});

</script>

</body>
</html>
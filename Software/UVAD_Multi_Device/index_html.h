/*
 * index_html.h — Multi-device UVAD controller web UI
 *
 * This page is fully dynamic: JavaScript fetches /devices every 750 ms
 * and builds a card for each discovered motor.  No template processor
 * is used, so the % character is safe in CSS / JS.
 */

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>UVAD Controller</title>
  <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
  <style>
    *{box-sizing:border-box;margin:0;padding:0}
    body{font-family:"Lato",sans-serif;background:#232324;color:#fff;padding:10px;
         max-width:480px;margin:0 auto;-webkit-user-select:none;user-select:none}
    h1{text-align:center;color:#fc4903;font-family:Tahoma,sans-serif;font-weight:normal;
       font-size:36px;margin:8px 0}
    hr{border:none;border-top:1px solid #444;margin:8px 0}

    /* ---------- Device card ---------- */
    .card{background:#2e2e2e;border-radius:15px;padding:14px 16px;margin:10px 0}
    .card.offline{opacity:.45;pointer-events:none}
    .ch{display:flex;align-items:center;margin-bottom:6px}
    .dot{width:10px;height:10px;border-radius:50%;margin-right:8px;flex-shrink:0}
    .dot.on{background:#2ecc71}
    .dot.off{background:#e74c3c}
    .dn{font-size:18px;font-weight:bold;cursor:pointer;flex-grow:1}
    .hub{font-size:11px;color:#999;margin-left:6px}

    /* ---------- Stats ---------- */
    .cs{font-size:13px;color:#ccc;margin-bottom:6px;line-height:1.3;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
    .sok{color:#2ecc71;font-weight:bold}
    .swarn{color:#f1c40f;font-weight:bold}
    .serr{color:#e74c3c;font-weight:bold}

    /* ---------- Angle row ---------- */
    .ca{font-size:14px;margin-bottom:6px}
    .mb{background:#3b3b3b;border:1px solid #5c5c5c;color:#fff;padding:3px 8px;
        border-radius:5px;font-size:11px;cursor:pointer;margin-left:6px}

    /* ---------- Slider ---------- */
    .sd{display:flex;justify-content:space-between;font-size:24px;line-height:1;margin-bottom:2px}
    .sl{-webkit-appearance:none;appearance:none;width:100%;height:36px;
        background:#3b3b3b;border-radius:8px;outline:none;opacity:.9;margin:0}
    .sl::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:28px;
        height:28px;border-radius:50%;background:#fc4903;cursor:pointer}

    /* ---------- Position buttons ---------- */
    .pb{display:flex;justify-content:center;gap:4px;margin:8px 0;flex-wrap:wrap}
    .pb-btn{background:#fc4903;border:none;color:#fff;padding:8px 12px;
            border-radius:8px;font-size:14px;cursor:pointer;touch-action:manipulation}
    .pb-btn:active{opacity:.7}

    /* ---------- Stop buttons ---------- */
    .stop-btn{background:#d32f2f;border:none;color:#fff;padding:10px 0;width:100%;
              border-radius:8px;font-size:16px;cursor:pointer;margin-top:6px;
              touch-action:manipulation}
    .stop-btn:active{opacity:.7}
    .stop-all{background:#d32f2f;border:none;color:#fff;padding:14px 0;width:100%;
              border-radius:10px;font-size:18px;cursor:pointer;margin:12px 0;
              touch-action:manipulation}
    .stop-all:active{opacity:.7}

    /* ---------- Empty state ---------- */
    #no-dev{text-align:center;color:#777;padding:40px 0;font-size:16px}
  </style>
</head>
<body>
  <h1>UVAD Controller</h1>
  <hr>
  <div id="devices">
    <div id="no-dev">Scanning for devices&hellip;</div>
  </div>
  <button class="stop-all" onclick="stopAll()">STOP ALL</button>

<script>
/* ---- state ---- */
var cfg=null, known={};

/* ---- poll /devices ---- */
function poll(){
  var x=new XMLHttpRequest();
  x.timeout=2000;
  x.onreadystatechange=function(){
    if(x.readyState===4&&x.status===200){
      try{var d=JSON.parse(x.responseText);cfg=d.cfg;render(d.devs)}catch(e){}
    }
  };
  x.open('GET','/devices',true);
  x.send();
}

/* ---- render / update cards ---- */
function render(devs){
  var c=document.getElementById('devices');
  var nd=document.getElementById('no-dev');
  if(devs.length>0&&nd)nd.remove();

  var ids={};
  devs.forEach(function(d){
    ids[d.id]=true;
    var el=document.getElementById('d-'+d.id);
    if(!el){el=mkCard(d);c.appendChild(el);known[d.id]=true;}
    updCard(el,d);
  });

  for(var id in known){
    if(!ids[id]){var e=document.getElementById('d-'+id);if(e)e.remove();delete known[id];}
  }
}

/* ---- create a device card ---- */
function mkCard(d){
  var el=document.createElement('div');
  el.className='card';el.id='d-'+d.id;
  var sa=cfg?cfg.sa:22.5, la=cfg?cfg.la:45;
  el.innerHTML=
    '<div class="ch">'+
      '<span class="dot" id="dot-'+d.id+'"></span>'+
      '<span class="dn" onclick="ren(\''+d.id+'\')" id="nm-'+d.id+'"></span>'+
      '<span class="hub" id="hb-'+d.id+'"></span>'+
    '</div>'+
    '<div class="cs">'+
      '<span id="v-'+d.id+'"></span> | <span id="pg-'+d.id+'"></span> | <span id="dr-'+d.id+'"></span> | <span id="st-'+d.id+'"></span>'+
    '</div>'+
    '<div class="ca">Angle: <span id="an-'+d.id+'">--</span> '+
      '<button class="mb" onclick="cmd(\''+d.id+'\',\'zero\',0)">Zero</button></div>'+
    '<div class="sd"><span>-</span><span>+</span></div>'+
    '<input type="range" class="sl" id="sl-'+d.id+'" min="-320" max="320" value="0">'+
    '<div class="pb">'+
      '<button class="pb-btn" onclick="cmd(\''+d.id+'\',\'position\',1)">-'+la+'&deg;</button>'+
      '<button class="pb-btn" onclick="cmd(\''+d.id+'\',\'position\',2)">-'+sa+'&deg;</button>'+
      '<button class="pb-btn" onclick="cmd(\''+d.id+'\',\'position\',3)">+'+sa+'&deg;</button>'+
      '<button class="pb-btn" onclick="cmd(\''+d.id+'\',\'position\',4)">+'+la+'&deg;</button>'+
    '</div>'+
    '<button class="stop-btn" onclick="cmd(\''+d.id+'\',\'stop\',0)">STOP</button>';

  initSlider(el.querySelector('.sl'),d.id);
  return el;
}

/* ---- update card values ---- */
function updCard(el,d){
  el.className='card'+(d.on?'':' offline');
  document.getElementById('dot-'+d.id).className='dot '+(d.on?'on':'off');
  document.getElementById('nm-'+d.id).textContent=d.n;
  document.getElementById('hb-'+d.id).textContent=d.hub?' (Hub)':'';
  document.getElementById('v-'+d.id).textContent=(d.mv/1000).toFixed(2)+'V';

  var pg=document.getElementById('pg-'+d.id);
  pg.textContent=d.pg===0?'Power Good':'Power Bad';
  pg.className=d.pg===0?'sok':'serr';

  var dr=document.getElementById('dr-'+d.id);
  dr.textContent=['Driver No Errors','Driver Temp Warning','Driver Temp Shutdown','Driver HW Disabled'][d.dr]||'?';
  dr.className=d.dr===0?'sok':d.dr===1?'swarn':'serr';

  var st=document.getElementById('st-'+d.id);
  st.textContent=d.st===0?'Not Stalled':'Stalled';
  st.className=d.st===0?'sok':'serr';

  document.getElementById('an-'+d.id).textContent=d.ang+'\u00B0';
}

/* ---- velocity slider with snap-to-centre ---- */
function initSlider(sl,id){
  var last=0;

  sl.addEventListener('input',function(){
    var now=Date.now();
    if(now-last<100)return;
    last=now;
    cmd(id,'velocity',parseInt(sl.value));
  });

  sl.addEventListener('pointerdown',function(e){
    sl.setPointerCapture(e.pointerId);
  });

  function snap(e){
    try{if(sl.hasPointerCapture(e.pointerId))sl.releasePointerCapture(e.pointerId);}catch(x){}
    sl.value=0;
    cmd(id,'velocity',0);
  }
  sl.addEventListener('pointerup',snap);
  sl.addEventListener('pointercancel',snap);

  /* safety: if touch leaves the slider area */
  sl.addEventListener('touchend',function(){sl.value=0;cmd(id,'velocity',0);},{passive:true});
}

/* ---- send command ---- */
function cmd(id,c,v){
  var x=new XMLHttpRequest();
  x.open('POST','/command',true);
  x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
  x.send('id='+id+'&cmd='+c+'&val='+(v||0));

  if(c==='zero'){
    var angleEl=document.getElementById('an-'+id);
    if(angleEl) angleEl.textContent='0\u00B0';
  }
}

/* ---- rename device ---- */
function ren(id){
  var el=document.getElementById('nm-'+id);
  var n=prompt('Rename device:',el.textContent);
  if(n&&n.trim()){
    var x=new XMLHttpRequest();
    x.open('POST','/rename',true);
    x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
    x.send('id='+id+'&name='+encodeURIComponent(n.trim()));
  }
}

/* ---- stop all devices ---- */
function stopAll(){
  var x=new XMLHttpRequest();
  x.open('POST','/stop_all',true);
  x.send();
  document.querySelectorAll('.sl').forEach(function(s){s.value=0;});
}

/* ---- start polling ---- */
setInterval(poll,150);
poll();
</script>
</body>
</html>
)rawliteral";

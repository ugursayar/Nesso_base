#pragma once

static const char WEB_FM_HTML[] PROGMEM = R"WEBFM(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Nesso N1 Files</title>
<style>
*{box-sizing:border-box}
body{font-family:monospace;background:#111827;color:#d1d5db;margin:0;padding:12px;font-size:13px}
h2{color:#38bdf8;margin:0 0 6px;font-size:16px}
nav{margin:4px 0 10px;color:#6b7280}
nav a{color:#38bdf8;text-decoration:none;cursor:pointer}
nav a:hover{text-decoration:underline}
.bar{display:flex;gap:6px;flex-wrap:wrap;align-items:center;margin-bottom:8px}
input,select{background:#1f2937;border:1px solid #374151;color:#d1d5db;padding:3px 7px;font:13px monospace;border-radius:3px;outline:none}
input:focus,select:focus{border-color:#38bdf8}
button{background:#1f2937;border:1px solid #374151;color:#d1d5db;padding:3px 10px;cursor:pointer;font:13px monospace;border-radius:3px}
button:hover{border-color:#38bdf8;color:#38bdf8}
table{width:100%;border-collapse:collapse}
th{text-align:left;color:#6b7280;border-bottom:1px solid #1f2937;padding:3px 6px}
td{padding:3px 6px;border-bottom:1px solid #1a2234}
tr.sel td{background:#1e293b}
.dn{color:#38bdf8;cursor:pointer}
.dn:hover{text-decoration:underline}
.sz{color:#6b7280;text-align:right;white-space:nowrap}
.ac a{color:#6b7280;text-decoration:none;margin-left:8px}
.ac a:hover{color:#d1d5db}
.del{color:#f87171!important}
.del:hover{color:#fca5a5!important}
.st{color:#6b7280;font-size:11px;margin:6px 0;min-height:15px}
.err{color:#f87171}
#sp{display:none;background:#0f172a;border:1px solid #1e293b;border-radius:4px;padding:12px;margin-bottom:10px}
.sph{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px}
.sph h3{margin:0;color:#38bdf8;font-size:14px}
.sg{margin-bottom:12px}
.sg h4{margin:0 0 6px;color:#94a3b8;font-size:11px;text-transform:uppercase;letter-spacing:.05em;border-bottom:1px solid #1e293b;padding-bottom:4px}
.sr{display:flex;align-items:center;gap:8px;margin-bottom:5px}
.sr label{min-width:120px;color:#9ca3af;font-size:12px}
.sr input,.sr select{flex:1;max-width:220px}
#si{background:#0a0f1a;border:1px solid #1e293b;border-radius:3px;padding:7px 10px;font-size:12px;margin-bottom:12px;line-height:1.8}
.sik{color:#6b7280}
.siv{color:#e2e8f0}
.snote{color:#6b7280;font-size:11px;margin-top:4px}
</style>
</head>
<body>
<h2>&#128193; Nesso N1 &mdash; File Manager</h2>
<nav id="bc"></nav>
<div class="bar">
  <label style="cursor:pointer;background:#1f2937;border:1px solid #374151;padding:3px 10px;border-radius:3px;color:#d1d5db" onmouseenter="this.style.borderColor='#38bdf8'" onmouseleave="this.style.borderColor='#374151'">
    &#8593; Upload<input type="file" id="uf" multiple style="display:none" onchange="doUpload()">
  </label>
  <button onclick="mkdirUI()">+ Folder</button>
  <span style="color:#374151">|</span>
  <input id="nn" placeholder="new name" style="width:150px">
  <button onclick="doRename()">Rename</button>
  <span style="color:#374151">|</span>
  <input id="pb" placeholder="/irdb/..." style="width:180px">
  <button onclick="go(document.getElementById('pb').value)">Go</button>
  <span style="color:#374151">|</span>
  <button id="stb" onclick="toggleSettings()">&#9881; Device</button>
</div>
<div id="sp">
  <div class="sph">
    <h3>&#9881; Device Settings</h3>
    <button onclick="toggleSettings()">&#10005;</button>
  </div>
  <div id="si">Loading...</div>
  <div class="sg">
    <h4>WiFi</h4>
    <div class="sr"><label>SSID</label><input id="s_wifi_ssid" placeholder="SSID"></div>
    <div class="sr"><label>Password</label><input id="s_wifi_pass" placeholder="password"></div>
    <p class="snote">WiFi changes take effect on reboot.</p>
  </div>
  <div class="sg">
    <h4>Network &amp; Time</h4>
    <div class="sr"><label>NTP Server</label><input id="s_ntp_server" placeholder="pool.ntp.org"></div>
    <div class="sr"><label>UTC Offset (h)</label><input id="s_gmt_offset" type="number" min="-12" max="14" style="max-width:80px"></div>
    <div class="sr"><label>DST Offset (s)</label><input id="s_dst_offset" type="number" style="max-width:80px"></div>
  </div>
  <div class="sg">
    <h4>Robot UDP</h4>
    <div class="sr"><label>Robot IP</label><input id="s_robot_ip" placeholder="192.168.1.27"></div>
    <div class="sr"><label>UDP Port</label><input id="s_udp_port" type="number" style="max-width:80px"></div>
  </div>
  <div class="sg">
    <h4>Power</h4>
    <div class="sr"><label>Dim Timeout</label>
      <select id="s_dim">
        <option value="0">30s</option><option value="1">60s</option>
        <option value="2">2min</option><option value="3">OFF</option>
      </select>
    </div>
    <div class="sr"><label>Sleep Timeout</label>
      <select id="s_sleep">
        <option value="0">2min</option><option value="1">5min</option>
        <option value="2">10min</option><option value="3">OFF</option>
      </select>
    </div>
    <div class="sr"><label>Low Battery</label>
      <select id="s_lowbat">
        <option value="0">5%</option><option value="1">10%</option>
        <option value="2">OFF</option>
      </select>
    </div>
    <div class="sr"><label>UI Clicks</label>
      <select id="s_click"><option value="1">ON</option><option value="0">OFF</option></select>
    </div>
    <div class="sr"><label>RF433 Module</label>
      <select id="s_rf433"><option value="1">ON</option><option value="0">OFF</option></select>
    </div>
    <div class="sr"><label>Speaker Hat</label>
      <select id="s_spk"><option value="1">ON</option><option value="0">OFF</option></select>
    </div>
    <div class="sr"><label>Volume</label>
      <select id="s_spkvol">
        <option value="1">25%</option><option value="2">50%</option>
        <option value="3">75%</option><option value="4">100%</option>
      </select>
    </div>
  </div>
  <div style="display:flex;gap:8px;align-items:center">
    <button onclick="saveDevSettings()" style="background:#1e3a5f;border-color:#38bdf8;color:#38bdf8">Save</button>
    <span id="sst" class="st" style="margin:0"></span>
  </div>
</div>
<div class="st" id="st"></div>
<table>
<thead><tr><th>Name</th><th class="sz">Size</th><th>Actions</th></tr></thead>
<tbody id="fl"></tbody>
</table>
<script>
var cwd='/',sel=null;
function $(id){return document.getElementById(id);}
function st(m,e){var el=$('st');el.textContent=m;el.className='st'+(e?' err':'');}
function sz(n){return n<1024?n+'B':n<1048576?(n/1024).toFixed(1)+'K':(n/1048576).toFixed(1)+'M';}
function jp(a,b){return(a==='/'?'':a.replace(/\/+$/,''))+'/'+b;}
function go(p){cwd=p||'/';$('pb').value=cwd;load();}
function ea(s){return s.replace(/\\/g,'\\\\').replace(/'/g,"\\x27");}
function he(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');}
function bc(){
  var parts=cwd.split('/').filter(Boolean),h='<a onclick="go(\'/\')">~</a>',pp='';
  parts.forEach(function(x){pp+='/'+x;var cp=pp;h+=' / <a onclick="go(\''+ea(cp)+'\')">'+he(x)+'</a>';});
  $('bc').innerHTML=h;
}
function load(){
  bc();sel=null;$('nn').value='';
  fetch('/api/ls?path='+encodeURIComponent(cwd)).then(function(r){return r.json();}).then(function(d){
    if(d.error){st(d.error,1);$('fl').innerHTML='';return;}
    var rows='',entries=d.entries||[];
    if(cwd!=='/'){var up=cwd.split('/').slice(0,-1).join('/')||'/';rows+='<tr><td><span class="dn" onclick="go(\''+ea(up)+'\')">&#8593; ..</span></td><td></td><td></td></tr>';}
    entries.sort(function(a,b){return (b.isDir-a.isDir)||a.name.localeCompare(b.name);});
    entries.forEach(function(e){
      var fp=jp(cwd,e.name),en=he(e.name),ep=ea(fp),en2=ea(e.name);
      if(e.isDir){
        rows+='<tr data-path="'+he(fp)+'"><td><span class="dn" onclick="go(\''+ep+'\')">&#128193; '+en+'</span></td><td></td>';
        rows+='<td class="ac"><a class="del" onclick="del(\''+ep+'\',1)">del</a></td></tr>';
      }else{
        rows+='<tr data-path="'+he(fp)+'" onclick="selRow(this,\''+en2+'\')"><td>'+en+'</td>';
        rows+='<td class="sz">'+sz(e.size)+'</td>';
        rows+='<td class="ac"><a href="/api/dl?path='+encodeURIComponent(fp)+'" download="'+en+'">dl</a>';
        rows+='<a class="del" onclick="del(\''+ep+'\',0)">del</a></td></tr>';
      }
    });
    $('fl').innerHTML=rows||'<tr><td colspan=3 style="color:#374151;padding:8px 6px">(empty)</td></tr>';
    st('');
  }).catch(function(e){st(''+e,1);});
}
function selRow(tr,name){
  var prev=document.querySelector('tr.sel');
  if(prev)prev.classList.remove('sel');
  if(sel===tr.dataset.path){sel=null;$('nn').value='';}
  else{tr.classList.add('sel');sel=tr.dataset.path;$('nn').value=name;}
}
function del(path,isDir){
  if(!confirm('Delete '+path+'?'))return;
  fetch('/api/rm?path='+encodeURIComponent(path),{method:'POST'}).then(function(r){return r.json();}).then(function(d){
    if(d.ok)load();else st(d.error,1);
  }).catch(function(e){st(''+e,1);});
}
function mkdirUI(){
  var n=prompt('Folder name:');if(!n)return;
  fetch('/api/mkdir?path='+encodeURIComponent(jp(cwd,n)),{method:'POST'}).then(function(r){return r.json();}).then(function(d){
    if(d.ok)load();else st(d.error,1);
  }).catch(function(e){st(''+e,1);});
}
function doRename(){
  var nn=$('nn').value.trim();
  if(!sel||!nn){st('Select a file first, then enter new name',1);return;}
  var to=jp(cwd,nn);
  fetch('/api/mv?from='+encodeURIComponent(sel)+'&to='+encodeURIComponent(to),{method:'POST'}).then(function(r){return r.json();}).then(function(d){
    if(d.ok){sel=null;$('nn').value='';load();}else st(d.error,1);
  }).catch(function(e){st(''+e,1);});
}
function doUpload(){
  var files=$('uf').files;if(!files.length)return;
  var arr=Array.from(files),i=0;
  (function next(){
    if(i>=arr.length){$('uf').value='';load();return;}
    var f=arr[i++],path=jp(cwd,f.name),fd=new FormData();
    fd.append('file',f);
    st('Uploading '+f.name+' ('+i+'/'+arr.length+')...');
    fetch('/api/upload?path='+encodeURIComponent(path),{method:'POST',body:fd}).then(function(r){return r.json();}).then(function(d){
      if(d.ok)next();else st(d.error,1);
    }).catch(function(e){st(''+e,1);});
  })();
}
var siTimer=null;
function toggleSettings(){
  var p=$('sp');
  if(p.style.display==='none'||!p.style.display){
    p.style.display='block';
    loadDevSettings();
    loadSysInfo();
    if(siTimer)clearInterval(siTimer);
    siTimer=setInterval(loadSysInfo,5000);
  }else{
    p.style.display='none';
    if(siTimer){clearInterval(siTimer);siTimer=null;}
  }
}
function loadDevSettings(){
  fetch('/api/settings').then(function(r){return r.json();}).then(function(d){
    $('s_wifi_ssid').value=d.wifi_ssid||'';
    $('s_wifi_pass').value=d.wifi_pass||'';
    $('s_ntp_server').value=d.ntp_server||'';
    $('s_gmt_offset').value=d.gmt_offset||0;
    $('s_dst_offset').value=d.dst_offset||0;
    $('s_robot_ip').value=d.robot_ip||'';
    $('s_udp_port').value=d.udp_port||8889;
    $('s_dim').value=d.dim_timeout||0;
    $('s_sleep').value=d.sleep_timeout||0;
    $('s_lowbat').value=d.low_bat||0;
    $('s_click').value=d.ui_click?'1':'0';
    $('s_rf433').value=d.rf433_on?'1':'0';
    $('s_spk').value=d.spk_on?'1':'0';
    $('s_spkvol').value=d.spk_vol||3;
  }).catch(function(e){$('sst').textContent='Load failed: '+e;$('sst').className='st err';});
}
function loadSysInfo(){
  fetch('/api/sysinfo').then(function(r){return r.json();}).then(function(d){
    var up=d.uptime_s,uph=Math.floor(up/3600),upm=Math.floor((up%3600)/60),ups=up%60;
    var upt=(uph?uph+'h ':'')+(upm?upm+'m ':'')+ups+'s';
    $('si').innerHTML=
      '<span class="sik">IP </span><span class="siv">'+he(d.ip)+'</span>&nbsp;&nbsp;'+
      '<span class="sik">WiFi </span><span class="siv">'+he(d.wifi_ssid)+'&nbsp;'+d.wifi_rssi+'dBm</span>&nbsp;&nbsp;'+
      '<span class="sik">Bat </span><span class="siv">'+d.battery_v.toFixed(2)+'V&nbsp;'+d.battery_pct+'%</span>&nbsp;&nbsp;'+
      '<span class="sik">FS </span><span class="siv">'+sz(d.fs_used)+' / '+sz(d.fs_total)+'</span>&nbsp;&nbsp;'+
      '<span class="sik">Up </span><span class="siv">'+upt+'</span>';
  }).catch(function(){});
}
function saveDevSettings(){
  var payload={
    wifi_ssid:$('s_wifi_ssid').value,
    wifi_pass:$('s_wifi_pass').value,
    ntp_server:$('s_ntp_server').value,
    gmt_offset:parseInt($('s_gmt_offset').value)||0,
    dst_offset:parseInt($('s_dst_offset').value)||0,
    robot_ip:$('s_robot_ip').value,
    udp_port:parseInt($('s_udp_port').value)||8889,
    dim_timeout:parseInt($('s_dim').value),
    sleep_timeout:parseInt($('s_sleep').value),
    low_bat:parseInt($('s_lowbat').value),
    ui_click:$('s_click').value==='1',
    rf433_on:$('s_rf433').value==='1',
    spk_on:$('s_spk').value==='1',
    spk_vol:parseInt($('s_spkvol').value)
  };
  $('sst').textContent='Saving...';$('sst').className='st';
  fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)})
    .then(function(r){return r.json();})
    .then(function(d){
      if(d.ok){$('sst').textContent='Saved.';$('sst').className='st';}
      else{$('sst').textContent=d.error||'Error';$('sst').className='st err';}
    }).catch(function(e){$('sst').textContent=''+e;$('sst').className='st err';});
}
load();
</script>
</body>
</html>
)WEBFM";

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
input{background:#1f2937;border:1px solid #374151;color:#d1d5db;padding:3px 7px;font:13px monospace;border-radius:3px;outline:none}
input:focus{border-color:#38bdf8}
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
load();
</script>
</body>
</html>
)WEBFM";

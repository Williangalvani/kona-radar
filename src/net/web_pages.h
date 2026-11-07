// Static HTML for the setup portal and the settings page. Colors follow the
// device palette in docs/design/DESIGN.md.
#pragma once

#include <Arduino.h>

namespace net {

#define RADAR_PAGE_STYLE                                                                                       \
  ":root{color-scheme:dark;--bg:#000;--panel:#0C171B;--line:#294148;--text:#F2F4EB;--muted:#97A7A9;"            \
  "--quiet:#64797C;--cyan:#80E1E8;--sage:#B8E3AD;--coral:#FF896D;--amber:#FFC271}"                              \
  "body{margin:0;font:17px/1.45 -apple-system,system-ui,sans-serif;background:var(--bg);color:var(--text)}"     \
  "main{max-width:460px;margin:0 auto;padding:22px 18px 60px}"                                                  \
  ".eyebrow{font:600 12px/1 ui-monospace,Menlo,monospace;letter-spacing:.16em;color:var(--muted);"              \
  "border-top:2px solid var(--cyan);display:inline-block;padding-top:10px}"                                     \
  "h1{font-size:26px;margin:10px 0 4px}h2{font-size:13px;font-family:ui-monospace,Menlo,monospace;"             \
  "letter-spacing:.12em;text-transform:uppercase;color:var(--muted);margin:26px 0 8px;font-weight:600}"          \
  "p.sub{margin:0 0 16px;color:var(--muted)}"                                                                   \
  ".card{border:1px solid var(--line);border-radius:14px;background:var(--panel);padding:4px 16px 14px}"        \
  "label{display:block;margin:12px 0 5px;font-size:14px;color:var(--muted)}"                                    \
  ".hint{font-size:13px;color:var(--quiet);margin-top:4px}"                                                     \
  "input,select{width:100%;box-sizing:border-box;padding:12px;font:inherit;border-radius:10px;"                 \
  "border:1px solid var(--line);background:#000;color:var(--text)}"                                             \
  "input:focus,select:focus{outline:none;border-color:var(--cyan)}"                                             \
  ".row{display:flex;gap:10px}.row>div{flex:1}"                                                                 \
  "button{font:inherit;border-radius:10px;padding:12px 16px;border:1px solid var(--line);background:var(--panel);" \
  "color:var(--text)}"                                                                                          \
  "button.main{width:100%;margin-top:22px;padding:15px;font-weight:600;letter-spacing:.04em;"                   \
  "border:1px solid var(--cyan);color:var(--cyan)}button.main:disabled{opacity:.45}"                            \
  ".link{background:none;border:0;color:var(--cyan);padding:8px 0;text-decoration:underline}"                   \
  ".check{display:flex;align-items:center;gap:10px;color:var(--text)}.check input{width:auto}"                  \
  ".msg{margin-top:18px;padding:14px;border-radius:12px;display:none}"                                          \
  ".msg.err{display:block;color:var(--coral);border:1px solid var(--coral)}"                                    \
  ".msg.ok{display:block;color:var(--sage);border:1px solid var(--sage)}"                                       \
  ".msg.info{display:block;color:var(--text);border:1px solid var(--line);background:var(--panel)}"

static const char kPortalPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kona Radar setup</title>
<style>)HTML" RADAR_PAGE_STYLE R"HTML(
.net{display:flex;align-items:center;justify-content:space-between;width:100%;box-sizing:border-box;
 padding:14px 16px;margin:0 0 8px;border-radius:12px;text-align:left}
.net.sel{border-color:var(--cyan)}
.bars{font-size:12px;color:var(--muted);white-space:nowrap;margin-left:10px;letter-spacing:1px}
</style></head><body><main>
<span class="eyebrow">FIRST FLIGHT</span>
<h1>Set up your radar</h1>
<p class="sub">Pick your home Wi-Fi. The radar needs a 2.4&nbsp;GHz network. Networks saved before are kept; this adds one.</p>
<h2>1 &middot; Wi-Fi</h2>
<div id="list"><p class="sub">Looking for networks&hellip;</p></div>
<button class="link" id="rescan">Search again</button> &nbsp;
<button class="link" id="other">Network not listed?</button>
<form id="f">
 <div id="ssidBox" style="display:none"><label for="ssid">Network name</label>
 <input id="ssid" autocapitalize="none" autocorrect="off" spellcheck="false" maxlength="32"></div>
 <label for="pass">Password</label>
 <input id="pass" type="password" autocomplete="current-password" maxlength="63">
 <label class="check"><input type="checkbox" id="showpw"> Show password</label>
 <h2>2 &middot; Your balcony <span style="text-transform:none;letter-spacing:0">(optional)</span></h2>
 <div class="card">
  <label for="obs">Location (latitude, longitude)</label>
  <input id="obs" placeholder="e.g. 19.70, -156.00" inputmode="decimal" autocomplete="off">
  <div class="hint">In Google or Apple Maps, long-press the spot where the radar hangs and copy the numbers.
  Leave empty to center the radar on the airport; you can add it later.</div>
  <label for="face">Your view faces</label>
  <select id="face"><option value="auto">toward the airport</option><option value="fixed">a compass bearing</option></select>
  <div id="bearingBox" style="display:none"><label for="brg">Bearing (degrees true, 0-359)</label>
  <input id="brg" type="number" min="0" max="359" inputmode="numeric"></div>
 </div>
 <button class="main" id="go" type="submit" disabled>CONNECT</button>
</form>
<div class="msg" id="msg"></div>
</main><script>
var sel='',manual=false,lost=0;
function $(i){return document.getElementById(i)}
function bars(r){return r>-55?'&#9679;&#9679;&#9679;&#9679;':r>-67?'&#9679;&#9679;&#9679;&#9675;':r>-78?'&#9679;&#9679;&#9675;&#9675;':'&#9679;&#9675;&#9675;&#9675;'}
function esc(s){return s.replace(/[&<>"]/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]})}
function msg(cls,t){var m=$('msg');m.className='msg '+cls;m.innerHTML=t}
function upd(){$('go').disabled=!(manual?$('ssid').value.length:sel.length)}
function pair(v){var m=String(v).split(/[ ,;]+/).filter(Boolean).map(Number);return m.length==2&&!isNaN(m[0])&&!isNaN(m[1])&&Math.abs(m[0])<=90&&Math.abs(m[1])<=180?m:null}
function scan(){fetch('/api/scan').then(function(r){return r.json()}).then(function(j){
 var h='';j.networks.forEach(function(n){h+='<button type="button" class="net'+(n.ssid==sel?' sel':'')+'" data-s="'+esc(n.ssid)+'"><span>'+esc(n.ssid)+(n.secure?' &#128274;':'')+'</span><span class="bars">'+bars(n.rssi)+'</span></button>'});
 if(!h)h='<p class="sub">'+(j.scanning?'Looking for networks&hellip;':'No networks found.')+'</p>';
 $('list').innerHTML=h;
 Array.prototype.forEach.call(document.querySelectorAll('.net'),function(b){b.onclick=function(){sel=b.getAttribute('data-s');manual=false;$('ssidBox').style.display='none';mark();upd();$('pass').focus()}});
 if(j.scanning)setTimeout(scan,1500)}).catch(function(){setTimeout(scan,2000)})}
function mark(){Array.prototype.forEach.call(document.querySelectorAll('.net'),function(b){b.className='net'+(b.getAttribute('data-s')==sel?' sel':'')})}
$('rescan').onclick=function(){fetch('/api/scan',{method:'POST'}).then(function(){setTimeout(scan,500)})};
$('other').onclick=function(){manual=true;sel='';mark();$('ssidBox').style.display='block';$('ssid').focus();upd()};
$('ssid').oninput=upd;
$('showpw').onchange=function(){$('pass').type=this.checked?'text':'password'};
$('face').onchange=function(){$('bearingBox').style.display=this.value=='fixed'?'block':'none'};
function place(){var v=$('obs').value.trim();if(!v)return Promise.resolve();var o=pair(v);
 if(!o)return Promise.reject('The location should look like: 19.70, -156.00');
 var n={observerLat:o[0],observerLon:o[1],northUp:0,viewBearingDeg:$('face').value=='fixed'?((+$('brg').value||0)%360):-1};
 return fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(n)}).then(function(r){if(!r.ok)throw 'Could not save the location.'})}
$('f').onsubmit=function(e){e.preventDefault();var s=manual?$('ssid').value:sel,p=$('pass').value;
 if(p.length&&p.length<8){msg('err','Wi-Fi passwords are at least 8 characters.');return}
 $('go').disabled=true;
 place().then(function(){
  msg('info','Joining <b>'+esc(s)+'</b>&hellip; this takes up to 20 seconds. Watch the radar&rsquo;s screen too.');
  var b=new URLSearchParams();b.append('ssid',s);b.append('pass',p);
  return fetch('/api/wifi/connect',{method:'POST',body:b}).then(function(r){return r.json()}).then(function(j){
   if(!j.ok){msg('err',esc(j.error||'Could not start.'));upd();return}lost=0;poll()})
 }).catch(function(t){msg('err',typeof t=='string'?t:'The radar did not answer. Are you still on its Wi-Fi?');upd()})};
function poll(){fetch('/api/wifi/status').then(function(r){return r.json()}).then(function(j){lost=0;
 if(j.state=='trying'){setTimeout(poll,1000);return}
 if(j.state=='online'){msg('info','Joined <b>'+esc(j.ssid)+'</b>. Checking that flight data comes through&hellip;');setTimeout(poll,1000);return}
 if(j.state=='done'||j.state=='connected'){msg('ok','<b>You&rsquo;re connected.</b> Switch your phone back to your usual Wi-Fi. Settings live at <b>http://'+esc(j.ip||j.host)+'/</b>'+(j.ip?' (or http://'+esc(j.host)+'/ on computers and iPhones)':'')+'. The radar shows the address too: tap <b>MENU</b> on its screen.');return}
 msg('err',esc(j.error||'Could not connect.'));upd()}).catch(function(){
 if(++lost<6){setTimeout(poll,1500);return}
 msg('info','Your phone lost its connection to the radar. That usually means it worked &mdash; check the radar&rsquo;s screen.')})}
scan();
</script></body></html>
)HTML";

static const char kSettingsPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kona Radar settings</title>
<style>)HTML" RADAR_PAGE_STYLE R"HTML(
#status{font:13px/1.5 ui-monospace,Menlo,monospace;color:var(--muted);margin-top:6px}
.nrow{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:10px 0;border-bottom:1px solid var(--line)}
.nrow b{word-break:break-all}.nrow button{padding:8px 12px;font-size:14px}
#toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);padding:12px 18px;border-radius:10px;
 background:var(--panel);border:1px solid var(--cyan);display:none}
</style></head><body><main>
<span class="eyebrow">SETTINGS</span>
<h1>Kona Radar</h1><div id="status">Loading&hellip;</div>
<form id="f">
<h2>Your balcony</h2><div class="card">
 <label for="obs">Location (latitude, longitude)</label>
 <input id="obs" placeholder="e.g. 19.70, -156.00" inputmode="decimal" autocomplete="off">
 <div class="hint" id="obshint">In Google or Apple Maps, long-press the spot and copy the numbers. Empty = centered on the airport.</div>
 <label for="observerAltFt">Height above sea level (ft)</label><input id="observerAltFt" type="number" step="1">
</div>
<h2>View</h2><div class="card">
 <label for="northUp">Top of the screen</label>
 <select id="northUp"><option value="0">the direction you face (view up)</option><option value="1">north (north up)</option></select>
 <label for="face">You face</label>
 <select id="face"><option value="auto">toward the airport (auto)</option><option value="fixed">a compass bearing</option></select>
 <div id="bearingBox"><label for="viewBearingDeg">Bearing (degrees true, 0-359)</label>
 <input id="viewBearingDeg" type="number" min="0" max="359"></div>
 <div class="hint" id="bhint"></div>
 <label for="rangeNm">Range</label><select id="rangeNm"></select>
 <label for="screenRotation">Screen rotation</label>
 <select id="screenRotation"><option value="0">0&deg; (normal)</option><option value="1">90&deg; (picture turned clockwise)</option>
 <option value="2">180&deg; (upside down)</option><option value="3">270&deg; (picture turned counter-clockwise)</option></select>
 <div class="hint">For the way the radar is mounted; applies right away. Also on the radar: MENU &rsaquo; Screen rotation. The radar&rsquo;s own up (view or north) stays as set above.</div>
</div>
<h2>Alerts</h2><div class="card">
 <label class="check"><input type="checkbox" id="alerts"> Full-screen &ldquo;look&rdquo; alert about a minute before a likely landing</label>
</div>
<h2>Screen</h2><div class="card">
 <label for="brightness">Brightness (%)</label><input id="brightness" type="number" min="5" max="100">
 <div class="row"><div><label for="idleDimS">Quiet screen after (s, 0 = never)</label><input id="idleDimS" type="number" min="0"></div>
 <div><label for="idleBrightness">Quiet brightness (%)</label><input id="idleBrightness" type="number" min="0" max="100"></div></div>
 <label for="blackoutS">Screen off after quiet for (s, 0 = never)</label><input id="blackoutS" type="number" min="0">
 <label for="landingWakeMin">Wake for landings (minutes before touchdown, 0 = off)</label><input id="landingWakeMin" type="number" min="0" max="30">
 <div class="hint">Turns the screen on &mdash; even after a swipe down or screen-off &mdash; when a plane looks set to land within this time, and keeps it on until it does. Night brightness still applies.</div>
 <label for="wakeRangeNm">Also wake when any plane is within (NM of you, 0 = off)</label><input id="wakeRangeNm" type="number" min="0">
 <label for="nightMode">Night colors</label>
 <select id="nightMode"><option value="0">by the hours below</option><option value="1">never</option><option value="2">always</option></select>
 <div class="row"><div><label for="nightStartH">Night from (hour)</label><input id="nightStartH" type="number" min="0" max="23"></div>
 <div><label for="nightEndH">until (hour)</label><input id="nightEndH" type="number" min="0" max="23"></div>
 <div><label for="nightBrightness">Night %</label><input id="nightBrightness" type="number" min="0" max="100"></div></div>
</div>
<h2>Airport &amp; data</h2><div class="card">
 <div class="row"><div><label for="ctr">Airport (latitude, longitude)</label><input id="ctr" inputmode="decimal"></div>
 <div style="flex:.45"><label for="centerName">Label</label><input id="centerName" maxlength="7"></div></div>
 <label for="source">Preferred source</label><select id="source"><option value="0">adsb.fi</option><option value="1">adsb.lol</option></select>
 <label for="refreshS">Refresh every (seconds)</label><input id="refreshS" type="number" min="2" max="300">
</div>
<button class="main" type="submit">SAVE</button>
</form>
<p><button id="reset" type="button">Restore factory settings</button></p>
<h2>Wi-Fi networks</h2><div class="card">
 <div id="netlist" class="hint">Loading&hellip;</div>
 <label for="nssid">Add a network</label>
 <input id="nssid" list="seen" autocapitalize="none" autocorrect="off" spellcheck="false" maxlength="32" placeholder="Network name">
 <datalist id="seen"></datalist>
 <input id="npass" type="password" maxlength="64" placeholder="Password (empty for an open network)" autocomplete="new-password" style="margin-top:8px">
 <button id="nadd" type="button" style="margin-top:10px">Add network</button>
 <div class="hint">The radar joins the strongest saved network in range and switches when one drops out. It keeps up to 8; adding another forgets the one used longest ago. Passwords are never shown here. Holding the screen for 5&nbsp;seconds forgets all of them and starts phone setup.</div>
</div>
<h2>Firmware</h2><div class="card">
 <p>Version <b id="fwv">&hellip;</b> &middot; <a href="/update" style="color:var(--cyan)">Update firmware</a></p>
 <div class="hint">Updates need the update password, shown on the radar: tap <b>MENU</b>, then <b>Settings on phone</b>.</div>
</div>
<div id="toast"></div>
</main><script>
var S={},RANGES=%RANGES%,NUM=['observerAltFt','brightness','idleDimS','idleBrightness','blackoutS','landingWakeMin','wakeRangeNm','nightStartH','nightEndH','nightBrightness','refreshS'];
function $(i){return document.getElementById(i)}
function toast(t){var e=$('toast');e.textContent=t;e.style.display='block';setTimeout(function(){e.style.display='none'},2500)}
function pair(v){var m=String(v).split(/[ ,;]+/).filter(Boolean).map(Number);return m.length==2&&!isNaN(m[0])&&!isNaN(m[1])&&Math.abs(m[0])<=90&&Math.abs(m[1])<=180?m:null}
function brg(a,b,c,d){var r=Math.PI/180,y=Math.sin((d-b)*r)*Math.cos(c*r),x=Math.cos(a*r)*Math.sin(c*r)-Math.sin(a*r)*Math.cos(c*r)*Math.cos((d-b)*r);return(Math.atan2(y,x)/r+360)%360}
function dist(a,b,c,d){var r=Math.PI/180,h=Math.pow(Math.sin((c-a)*r/2),2)+Math.cos(a*r)*Math.cos(c*r)*Math.pow(Math.sin((d-b)*r/2),2);return 2*3440.065*Math.asin(Math.sqrt(h))}
function hint(){var o=pair($('obs').value),c=pair($('ctr').value);
 $('bhint').textContent=o&&c?'From your balcony the airport is '+dist(o[0],o[1],c[0],c[1]).toFixed(1)+' NM away at '+Math.round(brg(o[0],o[1],c[0],c[1]))+'\u00b0 true.':'';
 $('bearingBox').style.display=$('face').value=='fixed'?'block':'none'}
function fill(s){S=s;var set=Math.abs(s.observerLat-s.centerLat)>1e-6||Math.abs(s.observerLon-s.centerLon)>1e-6;
 $('obs').value=set?s.observerLat+', '+s.observerLon:'';$('ctr').value=s.centerLat+', '+s.centerLon;$('centerName').value=s.centerName;
 NUM.forEach(function(k){$(k).value=s[k]});
 $('rangeNm').innerHTML=RANGES.map(function(r){return'<option value="'+r+'"'+(r==s.rangeNm?' selected':'')+'>'+r+' NM</option>'}).join('');
 $('source').value=s.source;$('screenRotation').value=s.screenRotation;$('northUp').value=s.northUp?1:0;$('nightMode').value=s.nightMode;
 $('alerts').checked=!!s.alerts;$('face').value=s.viewBearingDeg<0?'auto':'fixed';$('viewBearingDeg').value=s.viewBearingDeg<0?'':s.viewBearingDeg;hint()}
function status(){fetch('/api/status').then(function(r){return r.json()}).then(function(j){
 $('fwv').textContent=j.version+(j.otaPending?' (new, confirming)':'');
 $('status').textContent=j.aircraft+' aircraft \u00b7 '+(j.feedOk?j.source+' live':'data unavailable'+(j.feedError?' ('+j.feedError+')':''))+' \u00b7 '+j.ssid+' '+j.rssi+' dBm \u00b7 v'+j.version}).catch(function(){})}
$('face').onchange=hint;$('obs').oninput=hint;$('ctr').oninput=hint;
$('f').onsubmit=function(e){e.preventDefault();var c=pair($('ctr').value),ov=$('obs').value.trim(),o=ov?pair(ov):c;
 if(!o||!c){toast('Locations look like: 19.70, -156.00');return}
 var n={observerLat:o[0],observerLon:o[1],centerLat:c[0],centerLon:c[1],centerName:$('centerName').value,rangeNm:+$('rangeNm').value,
  source:+$('source').value,screenRotation:+$('screenRotation').value,northUp:+$('northUp').value,nightMode:+$('nightMode').value,alerts:$('alerts').checked?1:0,
  viewBearingDeg:$('face').value=='auto'?-1:((+$('viewBearingDeg').value||0)%360)};
 NUM.forEach(function(k){if($(k).value!=='')n[k]=+$(k).value});
 fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(n)}).then(function(r){return r.json().then(function(j){if(!r.ok)throw j;fill(j);toast('Saved')})}).catch(function(j){toast((j&&j.error)||'Save failed')})};
$('reset').onclick=function(){if(!confirm('Restore factory settings? Wi-Fi stays connected.'))return;
 fetch('/api/reset',{method:'POST'}).then(function(r){return r.json()}).then(function(j){fill(j);toast('Factory settings restored')})};
function esc(s){return String(s).replace(/[&<>"]/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]})}
function post(u,o){var b=new URLSearchParams();for(var k in o)b.append(k,o[k]);
 return fetch(u,{method:'POST',body:b}).then(function(r){return r.json().then(function(j){if(!r.ok)throw j;return j})})}
function showNets(j){var h=j.networks.map(function(n){
  return '<div class="nrow"><div><b>'+esc(n.ssid)+'</b><div class="hint">'+(n.current?'connected now':n.rssi!==null?'in range \u00b7 '+n.rssi+' dBm':'not seen in the last scan')+(n.open?' \u00b7 open':'')+'</div></div>'+
   '<button type="button" class="rm" data-s="'+esc(n.ssid)+'" data-c="'+(n.current?1:0)+'">Remove</button></div>'}).join('');
 $('netlist').innerHTML=h||'No saved networks.';
 Array.prototype.forEach.call(document.querySelectorAll('.rm'),function(b){b.onclick=function(){var s=b.getAttribute('data-s');
  if(!confirm(b.getAttribute('data-c')=='1'?'The radar is using '+s+' right now. It stays connected until the connection drops or it restarts. Remove it?':'Remove '+s+'?'))return;
  post('/api/networks/remove',{ssid:s}).then(function(j){showNets(j);toast('Removed')}).catch(function(j){toast((j&&j.error)||'Failed')})}})}
function nets(){fetch('/api/networks').then(function(r){return r.json()}).then(showNets).catch(function(){})}
function seen(){fetch('/api/scan').then(function(r){return r.json()}).then(function(j){
 $('seen').innerHTML=j.networks.filter(function(n){return!n.saved}).map(function(n){return'<option value="'+esc(n.ssid)+'">'}).join('');
 if(j.scanning)setTimeout(seen,2000)}).catch(function(){})}
$('nssid').onfocus=function(){fetch('/api/scan',{method:'POST'}).then(function(){setTimeout(seen,2500)})};
$('nadd').onclick=function(){var s=$('nssid').value,p=$('npass').value;if(!s){toast('Enter the network name');return}
 post('/api/networks',{ssid:s,pass:p}).then(function(j){showNets(j);$('nssid').value='';$('npass').value='';toast('Saved '+s)}).catch(function(j){toast((j&&j.error)||'Failed')})};
fetch('/api/settings').then(function(r){return r.json()}).then(fill);status();setInterval(status,5000);nets();seen();setInterval(nets,15000);
</script></body></html>
)HTML";

static const char kUpdatePage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kona Radar firmware</title>
<style>)HTML" RADAR_PAGE_STYLE R"HTML(
.bar{height:10px;border-radius:5px;background:#000;border:1px solid var(--line);overflow:hidden;margin-top:14px;display:none}
.bar div{height:100%;width:0;background:var(--cyan)}
input[type=file]{padding:10px}
</style></head><body><main>
<span class="eyebrow">KONA RADAR</span>
<h1>Firmware update</h1>
<p class="sub">Running <b>%VERSION%</b> (slot %SLOT%). <a href="/" style="color:var(--cyan)">Back to settings</a></p>
<h2>Upload</h2><div class="card">
 <label for="file">Firmware file (<code>.pio/build/kona-radar/firmware.bin</code>)</label>
 <input id="file" type="file" accept=".bin,application/octet-stream">
 <label for="pw">Update password</label>
 <input id="pw" type="password" autocomplete="current-password" autocapitalize="none" spellcheck="false">
 <div class="hint">Shown on the radar: tap <b>MENU</b>, then <b>Settings on phone</b>.</div>
 <button class="main" id="go" type="button">Install</button>
 <div class="bar" id="bar"><div id="fill"></div></div>
 <div id="m" class="msg"></div>
 <div class="hint">The radar checks the file before switching to it. If the new firmware does not start properly, the radar goes back to the current one on its next restart.</div>
</div>
<h2>Change the update password</h2><div class="card">
 <label for="cur">Current password</label><input id="cur" type="password" autocomplete="current-password">
 <label for="np">New password (8&ndash;32 characters, no spaces)</label><input id="np" type="password" autocomplete="new-password">
 <button id="chg" type="button" style="margin-top:12px">Change password</button>
 <div id="m2" class="msg"></div>
</div>
<script>
function $(i){return document.getElementById(i)}
function esc(s){return String(s).replace(/[&<>"]/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]})}
function msg(id,k,h){var e=$(id);e.className='msg '+k;e.innerHTML=h}
function err(x){try{return JSON.parse(x.responseText).error}catch(e){return 'The radar did not answer.'}}
$('go').onclick=function(){var f=$('file').files[0];
 if(!f){msg('m','err','Choose the firmware file first.');return}
 if(!/\.bin$/i.test(f.name)){msg('m','err','The file should end in .bin.');return}
 var x=new XMLHttpRequest(),d=new FormData();d.append('firmware',f,f.name);
 x.open('POST','/update');x.setRequestHeader('X-Ota-Password',$('pw').value);x.setRequestHeader('X-Ota-Size',f.size);
 $('go').disabled=true;$('bar').style.display='block';msg('m','info','Uploading&hellip; keep this page open.');
 x.upload.onprogress=function(e){if(e.lengthComputable)$('fill').style.width=(100*e.loaded/e.total).toFixed(0)+'%'};
 x.onload=function(){$('go').disabled=false;
  if(x.status!=200){$('bar').style.display='none';msg('m','err',esc(err(x)));return}
  $('fill').style.width='100%';msg('m','info','Installed. The radar is restarting&hellip;');setTimeout(back,4000)};
 x.onerror=function(){$('go').disabled=false;msg('m','err','The upload was interrupted. Check the Wi-Fi and try again.')};
 x.send(d)};
var tries=0;
function back(){fetch('/api/status',{cache:'no-store'}).then(function(r){return r.json()}).then(function(j){
 msg('m','ok','Done &mdash; the radar now runs <b>'+esc(j.version)+'</b>.'+(j.otaPending?' It confirms the update after about 20 seconds online.':''))
}).catch(function(){if(++tries<30)setTimeout(back,2000);else msg('m','err','The radar has not come back yet. Look at its screen.')})}
$('chg').onclick=function(){var b=new URLSearchParams();b.append('current',$('cur').value);b.append('new',$('np').value);
 fetch('/api/ota/password',{method:'POST',body:b}).then(function(r){return r.json().then(function(j){
  if(r.ok){msg('m2','ok','Password changed. The radar shows the new one.');$('cur').value=$('np').value='';}
  else msg('m2','err',esc(j.error||'Failed'))})}).catch(function(){msg('m2','err','The radar did not answer.')})};
</script></main></body></html>
)HTML";

#undef RADAR_PAGE_STYLE

}  // namespace net

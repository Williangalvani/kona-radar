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
<p class="sub">Pick your home Wi-Fi. The radar needs a 2.4&nbsp;GHz network.</p>
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
 if(j.state=='done'||j.state=='connected'){msg('ok','<b>You&rsquo;re connected.</b> Switch your phone back to your usual Wi-Fi. Settings live at <b>http://'+esc(j.host)+'/</b> (or tap the header on the radar for a QR code).');return}
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
 <label for="screenRotation">Screen mounting</label>
 <select id="screenRotation"><option value="0">normal</option><option value="1">rotated 90&deg; clockwise</option>
 <option value="2">upside down</option><option value="3">rotated 270&deg; clockwise</option></select>
</div>
<h2>Alerts</h2><div class="card">
 <label class="check"><input type="checkbox" id="alerts"> Full-screen &ldquo;look&rdquo; alert about a minute before a likely landing</label>
</div>
<h2>Screen</h2><div class="card">
 <label for="brightness">Brightness (%)</label><input id="brightness" type="number" min="5" max="100">
 <div class="row"><div><label for="idleDimS">Quiet screen after (s, 0 = never)</label><input id="idleDimS" type="number" min="0"></div>
 <div><label for="idleBrightness">Quiet brightness (%)</label><input id="idleBrightness" type="number" min="0" max="100"></div></div>
 <label for="blackoutS">Screen off after quiet for (s, 0 = never)</label><input id="blackoutS" type="number" min="0">
 <label for="wakeRangeNm">Wake when a plane is within (NM of you, 0 = only for arrivals)</label><input id="wakeRangeNm" type="number" min="0">
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
<p class="hint">Wi-Fi: hold the screen for 5 seconds to forget the network and start phone setup again.</p>
<div id="toast"></div>
</main><script>
var S={},RANGES=%RANGES%,NUM=['observerAltFt','brightness','idleDimS','idleBrightness','blackoutS','wakeRangeNm','nightStartH','nightEndH','nightBrightness','refreshS'];
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
fetch('/api/settings').then(function(r){return r.json()}).then(fill);status();setInterval(status,5000);
</script></body></html>
)HTML";

#undef RADAR_PAGE_STYLE

}  // namespace net

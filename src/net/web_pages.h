// Static HTML for the setup portal and the settings page.
#pragma once

#include <Arduino.h>

namespace net {

static const char kPortalPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kona Radar Wi-Fi setup</title>
<style>
:root{color-scheme:dark}
body{margin:0;font:17px/1.4 -apple-system,system-ui,sans-serif;background:#000;color:#d8ffe6}
main{max-width:440px;margin:0 auto;padding:24px 18px 60px}
h1{font-size:24px;margin:0 0 4px;color:#3dff8b}
p.sub{margin:0 0 20px;color:#7fb394}
.net{display:flex;align-items:center;justify-content:space-between;width:100%;box-sizing:border-box;
 padding:14px 16px;margin:0 0 8px;border:1px solid #1d4a2d;border-radius:12px;background:#06180d;
 color:inherit;font:inherit;text-align:left}
.net.sel{border-color:#3dff8b;background:#0a2a17}
.bars{font-size:13px;color:#7fb394;white-space:nowrap;margin-left:10px}
label{display:block;margin:18px 0 6px;color:#7fb394;font-size:15px}
input{width:100%;box-sizing:border-box;padding:14px;font:inherit;border-radius:12px;border:1px solid #2e8a52;
 background:#020c06;color:#fff}
button.main{width:100%;margin-top:20px;padding:16px;font:inherit;font-weight:600;border:0;border-radius:12px;
 background:#3dff8b;color:#00210d}
button.main:disabled{opacity:.5}
.link{background:none;border:0;color:#3dff8b;font:inherit;padding:8px 0;text-decoration:underline}
.msg{margin-top:18px;padding:14px;border-radius:12px;display:none}
.msg.err{display:block;background:#2a0a0a;color:#ffb0b0;border:1px solid #803030}
.msg.ok{display:block;background:#0a2a17;color:#b8ffd2;border:1px solid #2e8a52}
.msg.info{display:block;background:#06180d;color:#d8ffe6;border:1px solid #1d4a2d}
.show{font-size:14px;color:#7fb394;margin-top:8px}
</style></head><body><main>
<h1>Kona Radar</h1>
<p class="sub">Choose your home Wi-Fi network. The radar only works with 2.4&nbsp;GHz networks.</p>
<div id="list"><p class="sub">Looking for networks&hellip;</p></div>
<button class="link" id="rescan">Search again</button> &nbsp;
<button class="link" id="other">Network not listed?</button>
<form id="f">
 <div id="ssidBox" style="display:none"><label for="ssid">Network name</label>
 <input id="ssid" autocapitalize="none" autocorrect="off" spellcheck="false" maxlength="32"></div>
 <label for="pass">Password</label>
 <input id="pass" type="password" autocomplete="current-password" maxlength="63">
 <label class="show"><input type="checkbox" id="showpw" style="width:auto"> Show password</label>
 <button class="main" id="go" type="submit" disabled>Connect</button>
</form>
<div class="msg" id="msg"></div>
</main><script>
var sel='',manual=false,polling=null,lost=0;
function $(i){return document.getElementById(i)}
function bars(r){return r>-55?'&#9679;&#9679;&#9679;&#9679;':r>-67?'&#9679;&#9679;&#9679;&#9675;':r>-78?'&#9679;&#9679;&#9675;&#9675;':'&#9679;&#9675;&#9675;&#9675;'}
function esc(s){return s.replace(/[&<>"]/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]})}
function msg(cls,t){var m=$('msg');m.className='msg '+cls;m.innerHTML=t}
function upd(){$('go').disabled=!(manual?$('ssid').value.length:sel.length)}
function scan(){fetch('/api/scan').then(function(r){return r.json()}).then(function(j){
 var h='';j.networks.forEach(function(n){h+='<button type="button" class="net'+(n.ssid==sel?' sel':'')+'" data-s="'+esc(n.ssid)+'"><span>'+esc(n.ssid)+(n.secure?' &#128274;':'')+'</span><span class="bars">'+bars(n.rssi)+'</span></button>'});
 if(!h)h='<p class="sub">'+(j.scanning?'Looking for networks&hellip;':'No networks found.')+'</p>';
 $('list').innerHTML=h;
 Array.prototype.forEach.call(document.querySelectorAll('.net'),function(b){b.onclick=function(){sel=b.getAttribute('data-s');manual=false;$('ssidBox').style.display='none';scanRender();upd();$('pass').focus()}});
 if(j.scanning)setTimeout(scan,1500)}).catch(function(){setTimeout(scan,2000)})}
function scanRender(){Array.prototype.forEach.call(document.querySelectorAll('.net'),function(b){b.className='net'+(b.getAttribute('data-s')==sel?' sel':'')})}
$('rescan').onclick=function(){fetch('/api/scan',{method:'POST'}).then(function(){setTimeout(scan,500)})};
$('other').onclick=function(){manual=true;sel='';scanRender();$('ssidBox').style.display='block';$('ssid').focus();upd()};
$('ssid').oninput=upd;
$('showpw').onchange=function(){$('pass').type=this.checked?'text':'password'};
$('f').onsubmit=function(e){e.preventDefault();var s=manual?$('ssid').value:sel,p=$('pass').value;
 if(p.length&&p.length<8){msg('err','Wi-Fi passwords are at least 8 characters.');return}
 $('go').disabled=true;msg('info','Connecting to <b>'+esc(s)+'</b>&hellip; this takes up to 20 seconds.');
 var b=new URLSearchParams();b.append('ssid',s);b.append('pass',p);
 fetch('/api/wifi/connect',{method:'POST',body:b}).then(function(r){return r.json()}).then(function(j){
  if(!j.ok){msg('err',esc(j.error||'Could not start.'));upd();return}lost=0;poll()}).catch(function(){msg('err','The radar did not answer. Are you still connected to its Wi-Fi?');upd()})};
function poll(){fetch('/api/wifi/status').then(function(r){return r.json()}).then(function(j){lost=0;
 if(j.state=='trying'){setTimeout(poll,1000);return}
 if(j.state=='done'||j.state=='connected'){msg('ok','<b>Connected!</b> The radar is starting up. You can close this page and reconnect your phone to your usual Wi-Fi.');return}
 msg('err',esc(j.error||'Could not connect.'));upd()}).catch(function(){
 if(++lost<6){setTimeout(poll,1500);return}
 msg('info','Your phone lost its connection to the radar. That usually means it worked &mdash; check the radar\'s screen.')})}
scan();
</script></body></html>
)HTML";

static const char kSettingsPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kona Radar settings</title>
<style>
:root{color-scheme:dark}
body{margin:0;font:16px/1.4 -apple-system,system-ui,sans-serif;background:#000;color:#d8ffe6}
main{max-width:560px;margin:0 auto;padding:22px 18px 60px}
h1{font-size:24px;margin:0;color:#3dff8b}
h2{font-size:15px;text-transform:uppercase;letter-spacing:.08em;color:#7fb394;margin:28px 0 8px}
.card{border:1px solid #1d4a2d;border-radius:14px;background:#04140a;padding:6px 16px 14px}
label{display:block;margin:12px 0 4px;font-size:14px;color:#9fd8b4}
.hint{font-size:13px;color:#6a9c7e;margin-top:3px}
input,select{width:100%;box-sizing:border-box;padding:11px;font:inherit;border-radius:10px;border:1px solid #2e8a52;background:#020c06;color:#fff}
.row{display:flex;gap:10px}.row>div{flex:1}
button{font:inherit;border-radius:10px;padding:12px 16px;border:1px solid #2e8a52;background:#0a2a17;color:#d8ffe6}
button.main{background:#3dff8b;color:#00210d;border:0;font-weight:600;width:100%;margin-top:22px;padding:15px}
#status{font-size:14px;color:#9fd8b4;margin-top:6px}
#toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);padding:12px 18px;border-radius:10px;background:#0a2a17;border:1px solid #3dff8b;display:none}
</style></head><body><main>
<h1>Kona Radar</h1><div id="status">Loading&hellip;</div>
<form id="f">
<h2>Where you are</h2><div class="card">
 <label for="obs">Balcony location (latitude, longitude)</label>
 <input id="obs" placeholder="19.700000, -156.000000" inputmode="decimal">
 <div class="hint">Tip: long-press the spot in Google Maps and copy the numbers it shows.</div>
 <label for="observerAltFt">Height above sea level (ft)</label><input id="observerAltFt" type="number" step="1">
</div>
<h2>Radar center</h2><div class="card">
 <div class="row"><div><label for="ctr">Center (latitude, longitude)</label><input id="ctr" inputmode="decimal"></div>
 <div style="flex:.45"><label for="centerName">Label</label><input id="centerName" maxlength="7"></div></div>
 <label for="rangeNm">Range</label><select id="rangeNm"></select>
</div>
<h2>Orientation</h2><div class="card">
 <label for="viewMode">Top of the screen points</label>
 <select id="viewMode"><option value="face">toward the radar center from your balcony (auto)</option>
 <option value="fixed">to a compass bearing I choose</option><option value="north">north</option></select>
 <div id="bearingBox"><label for="viewBearingDeg">Bearing at the top (degrees, 0-359)</label>
 <input id="viewBearingDeg" type="number" min="0" max="359"><div class="hint" id="bhint"></div></div>
 <label for="screenRotation">Screen mounting</label>
 <select id="screenRotation"><option value="0">USB port down (normal)</option><option value="1">rotated 90&deg;</option>
 <option value="2">upside down</option><option value="3">rotated 270&deg;</option></select>
</div>
<h2>Screen</h2><div class="card">
 <label for="brightness">Brightness (%)</label><input id="brightness" type="number" min="5" max="100">
 <div class="row"><div><label for="idleDimS">Dim after (seconds, 0 = never)</label><input id="idleDimS" type="number" min="0"></div>
 <div><label for="idleBrightness">Dimmed brightness (%)</label><input id="idleBrightness" type="number" min="0" max="100"></div></div>
 <label for="wakeRangeNm">Brighten when a plane is within (NM of you, 0 = off)</label><input id="wakeRangeNm" type="number" min="0">
 <div class="row"><div><label for="nightStartH">Night from (hour)</label><input id="nightStartH" type="number" min="0" max="23"></div>
 <div><label for="nightEndH">until (hour)</label><input id="nightEndH" type="number" min="0" max="23"></div>
 <div><label for="nightBrightness">Night %</label><input id="nightBrightness" type="number" min="0" max="100"></div></div>
</div>
<h2>Data</h2><div class="card">
 <label for="source">Preferred source</label><select id="source"><option value="0">adsb.fi</option><option value="1">adsb.lol</option></select>
 <label for="refreshS">Refresh every (seconds)</label><input id="refreshS" type="number" min="2" max="300">
</div>
<button class="main" type="submit">Save</button>
</form>
<p><button id="reset" type="button">Restore factory settings</button></p>
<div id="toast"></div>
</main><script>
var S={},RANGES=%RANGES%;
function $(i){return document.getElementById(i)}
function toast(t){var e=$('toast');e.textContent=t;e.style.display='block';setTimeout(function(){e.style.display='none'},2500)}
function pair(v){var m=String(v).split(/[ ,;]+/).filter(Boolean).map(Number);return m.length==2&&!isNaN(m[0])&&!isNaN(m[1])?m:null}
function brg(a,b,c,d){var r=Math.PI/180,y=Math.sin((d-b)*r)*Math.cos(c*r),x=Math.cos(a*r)*Math.sin(c*r)-Math.sin(a*r)*Math.cos(c*r)*Math.cos((d-b)*r);return(Math.atan2(y,x)/r+360)%360}
function dist(a,b,c,d){var r=Math.PI/180,h=Math.pow(Math.sin((c-a)*r/2),2)+Math.cos(a*r)*Math.cos(c*r)*Math.pow(Math.sin((d-b)*r/2),2);return 2*6371*Math.asin(Math.sqrt(h))}
function hint(){var o=pair($('obs').value),c=pair($('ctr').value);if(!o||!c){$('bhint').textContent='';return}
 $('bhint').textContent='From your balcony the center is '+dist(o[0],o[1],c[0],c[1]).toFixed(1)+' km away at '+Math.round(brg(o[0],o[1],c[0],c[1]))+'\u00b0.'}
function mode(){$('bearingBox').style.display=$('viewMode').value=='fixed'?'block':'none';hint()}
function fill(s){S=s;$('obs').value=s.observerLat+', '+s.observerLon;$('ctr').value=s.centerLat+', '+s.centerLon;
 ['observerAltFt','centerName','brightness','idleDimS','idleBrightness','wakeRangeNm','nightStartH','nightEndH','nightBrightness','refreshS','viewBearingDeg'].forEach(function(k){$(k).value=s[k]});
 $('rangeNm').innerHTML=RANGES.map(function(r){return'<option value="'+r+'"'+(r==s.rangeNm?' selected':'')+'>'+r+' NM</option>'}).join('');
 $('source').value=s.source;$('screenRotation').value=s.screenRotation;
 $('viewMode').value=s.viewBearingDeg<0?'face':s.viewBearingDeg==0?'north':'fixed';if(s.viewBearingDeg<=0)$('viewBearingDeg').value='';mode()}
function status(){fetch('/api/status').then(function(r){return r.json()}).then(function(j){
 $('status').textContent=j.aircraft+' aircraft \u00b7 data '+(j.feedOk?'live from '+j.source:'unavailable'+(j.feedError?' ('+j.feedError+')':''))+' \u00b7 Wi-Fi '+j.ssid+' ('+j.rssi+' dBm) \u00b7 v'+j.version}).catch(function(){})}
$('viewMode').onchange=mode;$('obs').oninput=hint;$('ctr').oninput=hint;
$('f').onsubmit=function(e){e.preventDefault();var o=pair($('obs').value),c=pair($('ctr').value);
 if(!o||!c){toast('Locations must look like: 19.7000, -156.0000');return}
 var n={observerLat:o[0],observerLon:o[1],centerLat:c[0],centerLon:c[1],centerName:$('centerName').value,rangeNm:+$('rangeNm').value,source:+$('source').value,screenRotation:+$('screenRotation').value};
 ['observerAltFt','brightness','idleDimS','idleBrightness','wakeRangeNm','nightStartH','nightEndH','nightBrightness','refreshS'].forEach(function(k){if($(k).value!=='')n[k]=+$(k).value});
 var m=$('viewMode').value;n.viewBearingDeg=m=='face'?-1:m=='north'?0:(+$('viewBearingDeg').value||0);
 fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(n)}).then(function(r){return r.json().then(function(j){if(!r.ok)throw j;fill(j);toast('Saved')})}).catch(function(j){toast((j&&j.error)||'Save failed')})};
$('reset').onclick=function(){if(!confirm('Restore factory settings? Wi-Fi stays connected.'))return;
 fetch('/api/reset',{method:'POST'}).then(function(r){return r.json()}).then(function(j){fill(j);toast('Factory settings restored')})};
fetch('/api/settings').then(function(r){return r.json()}).then(fill);status();setInterval(status,5000);
</script></body></html>
)HTML";

}  // namespace net

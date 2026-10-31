#pragma once
#include <pgmspace.h>

static const char PAGE_DASHBOARD[] PROGMEM = R"HTML(<!doctype html><html lang="nl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Terrarium</title>
<style>
body{font-family:sans-serif;margin:0;padding:1em;max-width:34em;background:#f4f6f4;color:#222}
h1{font-size:1.3em;margin:.2em 0}.card{background:#fff;border-radius:8px;padding:.8em;margin:.6em 0;box-shadow:0 1px 3px #0002}
.big{font-size:1.5em}.row{display:flex;align-items:center;justify-content:space-between;margin:.4em 0;flex-wrap:wrap;gap:.3em}
button{padding:.5em .8em;border:1px solid #888;border-radius:6px;background:#eee;font-size:1em}
button.on{background:#2e7d32;color:#fff}button.off{background:#c62828;color:#fff}button.auto{background:#1565c0;color:#fff}
.al{color:#b71c1c;font-weight:bold}.ok{color:#2e7d32}.bad{color:#b71c1c}a{color:#1565c0}
</style></head><body>
<h1>Terrarium</h1>
<div class="card"><div id="time">-</div>
<div class="big">Lucht <span id="t">-</span> &deg;C &nbsp; RV <span id="rh">-</span> %</div>
<div>Warmtebron <span id="ht">-</span> &deg;C &nbsp; <span id="dn"></span></div></div>
<div class="card"><b>Relais</b><div id="relays"></div><div id="msg"></div></div>
<div class="card"><b>Alarmen</b><div id="alarms">-</div></div>
<div class="card"><div>WiFi: <span id="wifi">-</span></div><div>GSM: <span id="gsm">-</span></div>
<div>SD: <span id="sd">-</span> &nbsp; Klok: <span id="rtc">-</span></div></div>
<p><a href="/settings">Instellingen</a> &nbsp; <a href="/logs">Logbestanden</a></p>
<script>
var NAMES={temp_low:"Temperatuur te laag",temp_high:"Temperatuur te hoog",source_hot:"Warmtebron te heet",rh_low:"Luchtvochtigheid te laag",rh_high:"Luchtvochtigheid te hoog",dht_fail:"Luchtsensor defect",ds_fail:"Bronsensor defect",sd_fail:"SD-kaart defect",rtc_fail:"Klok defect",wifi_lost:"WiFi weg"};
function $(i){return document.getElementById(i)}
function v(x,d){return x===null||x===undefined?"-":x}
function yn(b){return b?'<span class="ok">OK</span>':'<span class="bad">fout</span>'}
function cmd(r,m){
  fetch("/api/relay",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({relay:r,mode:m})})
  .then(function(x){$("msg").textContent=x.ok?"":(x.status==401?"Inloggen vereist":"Fout "+x.status);load()})
  .catch(function(){$("msg").textContent="Geen verbinding"})}
function load(){
  fetch("/api/status").then(function(r){return r.json()}).then(function(s){
    $("time").textContent=v(s.time).replace("T"," ");
    $("t").textContent=v(s.air_temp);$("rh").textContent=v(s.rh);$("ht").textContent=v(s.heater_temp);
    $("dn").textContent=s.is_day?"Dag":"Nacht";
    var h="";
    s.relays.forEach(function(r,i){
      var n=i+1,m=r.mode=="manual"?" (handmatig "+Math.ceil(r.manual_remaining_s/60)+" min)":"";
      h+='<div class="row"><span>'+r.name+': <b>'+(r.on?"AAN":"UIT")+'</b>'+m+'</span><span>'
      +'<button class="on" onclick="cmd('+n+',\'on\')">AAN</button> '
      +'<button class="off" onclick="cmd('+n+',\'off\')">UIT</button> '
      +'<button class="auto" onclick="cmd('+n+',\'auto\')">AUTO</button></span></div>'});
    $("relays").innerHTML=h;
    $("alarms").innerHTML=s.alarms.length?s.alarms.map(function(a){return '<div class="al">'+(NAMES[a]||a)+'</div>'}).join(""):'<span class="ok">Geen</span>';
    $("wifi").textContent=s.wifi.ok?s.wifi.ip+" ("+s.wifi.rssi+" dBm)":"niet verbonden";
    $("gsm").textContent=s.gsm.ok?"signaal "+s.gsm.signal+"/31":"geen";
    $("sd").innerHTML=yn(s.sd_ok);$("rtc").innerHTML=yn(s.rtc_ok);
  }).catch(function(){$("msg").textContent="Geen verbinding"})}
load();setInterval(load,5000);
</script></body></html>)HTML";

static const char PAGE_SETTINGS[] PROGMEM = R"HTML(<!doctype html><html lang="nl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Instellingen</title>
<style>
body{font-family:sans-serif;margin:0;padding:1em;max-width:34em;background:#f4f6f4;color:#222}
h1{font-size:1.3em}fieldset{background:#fff;border:1px solid #ccc;border-radius:8px;margin:.6em 0}
legend{font-weight:bold}label{display:flex;justify-content:space-between;align-items:center;margin:.35em 0;gap:.5em}
input{width:55%;padding:.35em;font-size:1em;box-sizing:border-box}input[type=checkbox]{width:auto}
button{padding:.6em 1em;border:1px solid #888;border-radius:6px;background:#eee;font-size:1em;margin:.2em 0}
button.p{background:#1565c0;color:#fff}#res{white-space:pre-wrap;margin:.6em 0;font-weight:bold}
</style></head><body>
<h1>Instellingen</h1><p><a href="/">&larr; Dashboard</a></p>
<form id="f" onsubmit="return save()"></form>
<div id="res"></div>
<fieldset><legend>Acties</legend>
<button onclick="setTime()">Klok zetten</button> <button onclick="sms()">Test-SMS</button></fieldset>
<script>
// [pad, label, type]  type: t=tekst, n=getal, p=wachtwoord (leeg = ongewijzigd), b=aan/uit, h=uu:mm
var G=[
["WiFi",[["wifi.ssid","SSID","t"],["wifi.password","Wachtwoord","p"],["wifi.hostname","Hostnaam","t"]]],
["Web",[["web.user","Gebruiker","t"],["web.password","Wachtwoord","p"]]],
["SMS",[["sms.phone","Telefoonnummer","t"],["sms.sim_pin","SIM-pincode","p"],["sms.limit_min","Min. minuten tussen SMS","n"]]],
["Dag/nacht",[["schedule.day_start","Dag begint","h"],["schedule.night_start","Nacht begint","h"]]],
["Verwarming",[["heating.day_c","Dag (C)","n"],["heating.night_c","Nacht (C)","n"],["heating.hyst_c","Hysterese (C)","n"],["heating.source_max_c","Bron max (C)","n"]]],
["Bevochtiging",[["humidity.on_below","Aan onder (%)","n"],["humidity.off_above","Uit boven (%)","n"],["humidity.max_on_min","Max aan (min)","n"],["humidity.pause_min","Pauze (min)","n"]]],
["Ventilatie",[["vent.on_above","Aan boven (%)","n"],["vent.off_below","Uit onder (%)","n"],["vent.interval_min","Interval (min)","n"],["vent.duration_min","Duur (min)","n"]]],
["Alarmen",[["alarms.temp_low_c","Temp laag (C)","n"],["alarms.temp_high_c","Temp hoog (C)","n"],["alarms.temp_delay_min","Temp vertraging (min)","n"],["alarms.rh_low","RV laag (%)","n"],["alarms.rh_high","RV hoog (%)","n"],["alarms.rh_delay_min","RV vertraging (min)","n"],["alarms.wifi_lost_min","WiFi weg na (min)","n"]]],
["Overig",[["manual_timeout_min","Handbediening time-out (min)","n"],["relay_active_low","Relais actief-laag","b"],["log.interval_min","Loginterval (min)","n"],["log.retention_days","Bewaartermijn (dagen)","n"],["ntp.server","NTP-server","t"],["ntp.tz","Tijdzone (POSIX)","t"]]]
];
function $(i){return document.getElementById(i)}
function get(o,p){p.split(".").forEach(function(k){o=o&&o[k]});return o}
function put(o,p,v){var a=p.split(".");for(var i=0;i<a.length-1;i++){o=o[a[i]]=o[a[i]]||{}}o[a[a.length-1]]=v}
var h="";
G.forEach(function(g,gi){h+="<fieldset><legend>"+g[0]+"</legend>";
 g[1].forEach(function(f,i){var id="f_"+gi+"_"+i,t=f[2];
  h+='<label for="'+id+'">'+f[1]+'<input id="'+id+'" data-p="'+f[0]+'" data-t="'+t+'" type="'+(t=="p"?"password":t=="n"?"number":t=="b"?"checkbox":t=="h"?"time":"text")+'"'+(t=="n"?' step="any"':'')+(t=="p"?' autocomplete="new-password" placeholder="(ongewijzigd)"':'')+'></label>'});
 h+="</fieldset>"});
h+='<button class="p" type="submit">Opslaan</button>';
$("f").innerHTML=h;
function inputs(){return document.querySelectorAll("#f input")}
function load(){fetch("/api/config").then(function(r){if(!r.ok)throw r.status;return r.json()}).then(function(c){
 inputs().forEach(function(e){var v=get(c,e.dataset.p);if(e.dataset.t=="p")return;
  if(e.dataset.t=="b")e.checked=!!v;else if(v!==undefined)e.value=v})
 }).catch(function(s){$("res").textContent="Laden mislukt ("+s+")"})}
function save(){var o={};
 inputs().forEach(function(e){var t=e.dataset.t,p=e.dataset.p;
  if(t=="p"){if(e.value!=="")put(o,p,e.value)}
  else if(t=="b")put(o,p,e.checked)
  else if(t=="n"){if(e.value!=="")put(o,p,Number(e.value))}
  else put(o,p,e.value)});
 post("/api/config",o,function(r){
  $("res").textContent=(r.ok?"Opgeslagen":"Fout: "+r.error)+(r.rejected?"\n"+r.rejected+" waarde(n) geweigerd"+(r.error?": "+r.error:""):"");
  load()});
 return false}
function post(u,o,cb){fetch(u,{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(o)})
 .then(function(r){return r.text().then(function(t){var j={};try{j=JSON.parse(t)}catch(e){j={ok:false,error:t}}
  if(r.status==403)j.ok=false;cb(j)})}).catch(function(){cb({ok:false,error:"Geen verbinding"})})}
function p2(n){return(n<10?"0":"")+n}
function setTime(){var d=new Date();
 var s=d.getFullYear()+"-"+p2(d.getMonth()+1)+"-"+p2(d.getDate())+"T"+p2(d.getHours())+":"+p2(d.getMinutes())+":"+p2(d.getSeconds());
 post("/api/time",{datetime:s},function(r){$("res").textContent=r.ok?"Klok gezet op "+s.replace("T"," "):"Fout: "+r.error})}
function sms(){post("/api/sms/test",{},function(r){$("res").textContent=r.ok?"Test-SMS wordt verstuurd":"Fout: "+r.error})}
load();
</script></body></html>)HTML";

static const char PAGE_SETUP[] PROGMEM = R"HTML(<!doctype html><html lang="nl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Terrarium setup</title>
<style>
body{font-family:sans-serif;margin:0;padding:1em;max-width:30em;background:#f4f6f4;color:#222}
label{display:block;margin:.8em 0}input{display:block;width:100%;padding:.5em;font-size:1em;box-sizing:border-box}
button{padding:.7em 1.2em;font-size:1em;background:#1565c0;color:#fff;border:0;border-radius:6px}
</style></head><body>
<h1>Terrarium instellen</h1>
<form onsubmit="return go()">
<label>WiFi-netwerk (SSID)<input id="s" required maxlength="32"></label>
<label>WiFi-wachtwoord<input id="p" type="password" maxlength="64" autocomplete="off"></label>
<label>Webwachtwoord (min. 6 tekens)<input id="w" type="password" required minlength="6" maxlength="64" autocomplete="new-password"></label>
<button type="submit">Opslaan en herstarten</button></form>
<p id="r"></p>
<script>
function go(){
 fetch("/api/setup",{method:"POST",headers:{"Content-Type":"application/json"},
  body:JSON.stringify({ssid:document.getElementById("s").value,password:document.getElementById("p").value,web_password:document.getElementById("w").value})})
 .then(function(r){return r.json()}).then(function(j){
  document.getElementById("r").textContent=j.ok?"Opgeslagen. Het apparaat herstart en maakt verbinding met je WiFi; sluit dit netwerk.":"Fout: "+j.error})
 .catch(function(){document.getElementById("r").textContent="Geen verbinding"});
 return false}
</script></body></html>)HTML";

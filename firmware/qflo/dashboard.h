#pragma once

// Self-contained dashboard served at "/". No CDN scripts or fonts: a phone on
// the device's own access point has no internet connection.

const char DASHBOARD_HTML[] PROGMEM = R"QFLO(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Q-FLO Monitor</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--fg:#1b1f24;--muted:#65707c;--line:#e3e6ea;--accent:#1f6feb;
--pure:#1f9d55;--slight:#c99a06;--moderate:#e8710a;--high:#d93025;--fault:#8e44ad;--idle:#6b7280}
@media (prefers-color-scheme:dark){:root{--bg:#0f1216;--card:#181c22;--fg:#e8eaed;--muted:#9aa4af;--line:#2a3038;--accent:#58a6ff}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
main{max-width:620px;margin:0 auto;padding:16px}
header{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px}
h1{font-size:20px;margin:0;letter-spacing:.5px}h1 span{color:var(--muted);font-weight:400;font-size:14px}
.pill{font-size:12px;padding:3px 10px;border-radius:99px;background:var(--line);color:var(--muted)}
.pill.on{background:#1f9d5522;color:var(--pure)}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px 16px;margin-bottom:12px}
.hero{color:#fff;border:0;text-align:center;padding:22px 16px;transition:background .3s}
.hero .label{font-size:26px;font-weight:700;letter-spacing:.5px}
.hero .sub{opacity:.9;font-size:14px;margin-top:4px}
.warn{background:#d9302518;border:1px solid var(--high);color:var(--high);display:none}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:12px}
.grid .card{margin:0}.k{color:var(--muted);font-size:13px}.v{font-size:24px;font-weight:650;font-variant-numeric:tabular-nums}
.v small{font-size:13px;color:var(--muted);font-weight:400;margin-left:3px}
h2{font-size:14px;margin:0 0 8px;color:var(--muted);font-weight:600}
canvas{width:100%;display:block}
.bar{display:flex;height:14px;border-radius:7px;overflow:hidden;background:var(--line)}
.legend{display:flex;flex-wrap:wrap;gap:4px 14px;margin-top:8px;font-size:13px;color:var(--muted)}
.legend i{display:inline-block;width:10px;height:10px;border-radius:3px;margin-right:5px;vertical-align:-1px}
details summary{cursor:pointer;font-weight:600}
label{display:block;font-size:13px;color:var(--muted);margin-top:10px}
input[type=number]{width:100%;padding:8px 10px;border:1px solid var(--line);border-radius:8px;background:var(--bg);color:var(--fg);font:inherit}
.row{display:grid;grid-template-columns:1fr 1fr;gap:10px}
button{font:inherit;padding:9px 14px;border-radius:9px;border:1px solid var(--line);background:var(--bg);color:var(--fg);cursor:pointer;margin-top:12px}
button.primary{background:var(--accent);border-color:var(--accent);color:#fff}
.prog{height:6px;background:var(--line);border-radius:3px;margin-top:10px;overflow:hidden}.prog div{height:100%;width:0;background:var(--accent)}
.note{font-size:13px;color:var(--muted);margin-top:6px}
footer{text-align:center;color:var(--muted);font-size:12px;margin:18px 0 8px}
</style></head><body><main>
<header><h1>Q-FLO <span>fuel monitor</span></h1><span id="live" class="pill">connecting…</span></header>

<section id="hero" class="card hero" style="background:var(--idle)">
  <div class="label" id="quality">—</div><div class="sub" id="state">Waiting for data</div>
</section>
<section id="warn" class="card warn">No signal from the fuel sensor. Check the 555 timer and sensor wiring.</section>

<div class="grid">
  <div class="card"><div class="k">Flow rate</div><div class="v" id="flow">—<small>L/min</small></div></div>
  <div class="card"><div class="k">This refuel</div><div class="v" id="total">—<small>L</small></div></div>
  <div class="card"><div class="k">Sensor frequency</div><div class="v" id="freq">—<small>Hz</small></div></div>
  <div class="card"><div class="k">Lifetime</div><div class="v" id="life">—<small>L</small></div></div>
</div>

<section class="card"><h2>Sensor frequency, last 2 min</h2><canvas id="cf" height="170"></canvas>
<h2 style="margin-top:12px">Flow rate</h2><canvas id="cq" height="70"></canvas></section>

<section class="card"><h2>This refuel: quality breakdown</h2><div class="bar" id="bar"></div><div class="legend" id="legend"></div>
<div class="note" id="avg"></div></section>

<section class="card"><details><summary>Calibration &amp; settings</summary>
  <label><input type="checkbox" id="bench"> Bench mode: classify without flow (static samples)</label>
  <button class="primary" id="calBtn">Capture pure-fuel reference (10 s)</button>
  <div class="prog"><div id="calProg"></div></div><div class="note" id="calMsg">Fill the sensor with known-pure fuel first.</div>
  <form id="thForm">
    <div class="row"><div><label>Pure band min (Hz)</label><input type="number" step="1" name="pureMin"></div>
    <div><label>Pure band max (Hz)</label><input type="number" step="1" name="pureMax"></div></div>
    <div class="row"><div><label>Slight up to (+Hz)</label><input type="number" step="1" name="slight"></div>
    <div><label>Moderate up to (+Hz)</label><input type="number" step="1" name="moderate"></div></div>
    <label>Flow pulses per litre</label><input type="number" step="0.01" name="ppl">
    <button class="primary" type="submit">Save thresholds</button>
    <button type="button" id="defaults">Restore defaults</button>
    <button type="button" id="reset">Reset this refuel</button>
    <div class="note" id="formMsg"></div>
  </form>
</details></section>
<footer id="foot"></footer>
</main>
<script>
const $=id=>document.getElementById(id);
const css=n=>getComputedStyle(document.documentElement).getPropertyValue(n).trim();
const COLOR={pure:'--pure',slight:'--slight',moderate:'--moderate',high:'--high',fault:'--fault',no_signal:'--idle',waiting:'--idle'};
const NAMES={pure:'Pure',slight:'Slight',moderate:'Moderate',high:'High',fault:'Sensor error',no_signal:'No signal'};
const STATE={fueling:'Refuelling in progress',done:'Result of the last refuel',idle:'Waiting for fuel flow'};
const hist={f:[],q:[]};let last=null,dirty=false;

function fmt(x,d){return Number(x).toLocaleString(undefined,{minimumFractionDigits:d,maximumFractionDigits:d})}
function setV(id,val,unit){$(id).innerHTML=val+'<small>'+unit+'</small>'}

async function poll(){
  const ctl=new AbortController();const t=setTimeout(()=>ctl.abort(),2500);
  try{const r=await fetch('/api/data',{cache:'no-store',signal:ctl.signal});render(await r.json());online(true)}
  catch(e){online(false)}finally{clearTimeout(t);setTimeout(poll,1000)}
}
function online(on){const p=$('live');p.textContent=on?'live':'offline';p.className='pill'+(on?' on':'')}

function render(d){
  last=d;
  $('hero').style.background='var('+(COLOR[d.class]||'--idle')+')';
  $('quality').textContent=d.quality;
  $('state').textContent=d.bench?'Bench mode: live reading':(STATE[d.state]||'');
  $('warn').style.display=d.signal?'none':'block';
  setV('flow',fmt(d.flow,2),'L/min');setV('total',fmt(d.total,3),'L');
  setV('freq',d.signal?fmt(d.freq,0):'—','Hz');setV('life',fmt(d.lifetime,1),'L');
  push(hist.f,d.signal?d.freq:null);push(hist.q,d.flow);
  drawFreq(d.th);drawLine($('cq'),hist.q,0,Math.max(5,...hist.q)*1.15,css('--accent'));
  breakdown(d.session);
  const c=d.cal;$('calProg').style.width=(c.state==='running'?c.progress:0)+'%';
  $('calBtn').disabled=c.state==='running';
  $('calMsg').textContent=c.state==='running'?'Capturing… '+c.progress+'%':
    c.state==='ok'?'Reference: '+fmt(c.mean,0)+' Hz ± '+fmt(c.sd,0)+' Hz ('+c.samples+' samples). Pure band updated.':
    c.state==='failed'?'Calibration failed: no stable sensor signal.':'Fill the sensor with known-pure fuel first.';
  $('bench').checked=d.bench;
  if(!dirty){const f=$('thForm');f.pureMin.value=Math.round(d.th.pureMin);f.pureMax.value=Math.round(d.th.pureMax);
    f.slight.value=Math.round(d.th.slight);f.moderate.value=Math.round(d.th.moderate);f.ppl.value=d.th.ppl}
  $('foot').textContent='Firmware '+d.fw+' · '+d.ap+(d.staIp?' · '+d.staIp:'')+' · up '+Math.floor(d.uptimeS/60)+' min';
}
function push(a,v){a.push(v);if(a.length>120)a.shift()}

function canvas(c){const r=window.devicePixelRatio||1,w=c.clientWidth,h=c.height/(c._r||1);
  if(c.width!==Math.round(w*r)){c._r=r;c.style.height=h+'px';c.width=Math.round(w*r);c.height=Math.round(h*r)}
  const g=c.getContext('2d');g.setTransform(r,0,0,r,0,0);g.clearRect(0,0,w,h);return{g,w,h}}

function drawLine(c,data,lo,hi,color,g0){
  const {g,w,h}=g0||canvas(c);const y=v=>h-4-(v-lo)/(hi-lo||1)*(h-8);
  g.strokeStyle=color;g.lineWidth=2;g.beginPath();let pen=false;
  data.forEach((v,i)=>{const x=i*(w/119);if(v==null){pen=false;return}pen?g.lineTo(x,y(v)):g.moveTo(x,y(v));pen=true});
  g.stroke();return{g,w,h,y}}

function drawFreq(th){
  const c=$('cf'),vals=hist.f.filter(v=>v!=null);const {g,w,h}=canvas(c);
  let lo=Math.min(th.pureMin,...vals),hi=Math.max(th.pureMax+th.slight,...vals);const pad=(hi-lo)*.08;lo-=pad;hi+=pad;
  const y=v=>h-4-(v-lo)/(hi-lo)*(h-8);
  const band=(a,b,col)=>{g.fillStyle=col+'26';g.fillRect(0,y(b),w,y(a)-y(b))};
  band(th.pureMin,th.pureMax,css('--pure'));band(th.pureMax,th.pureMax+th.slight,css('--slight'));
  band(th.pureMax+th.slight,th.pureMax+th.moderate,css('--moderate'));band(th.pureMax+th.moderate,hi,css('--high'));
  g.fillStyle=css('--muted');g.font='11px system-ui';g.fillText(fmt(hi,0)+' Hz',4,12);g.fillText(fmt(lo,0)+' Hz',4,h-6);
  drawLine(c,hist.f,lo,hi,css('--fg'),{g,w,h});
}

function breakdown(s){
  const keys=['pure','slight','moderate','high','fault','no_signal'];const tot=keys.reduce((a,k)=>a+s.counts[k],0);
  $('bar').innerHTML=tot?keys.filter(k=>s.counts[k]).map(k=>'<div style="width:'+(100*s.counts[k]/tot)+'%;background:var('+COLOR[k]+')"></div>').join(''):'';
  $('legend').innerHTML=tot?keys.filter(k=>s.counts[k]).map(k=>'<span><i style="background:var('+COLOR[k]+')"></i>'+NAMES[k]+' '+Math.round(100*s.counts[k]/tot)+'%</span>').join(''):'<span>No refuel recorded yet.</span>';
  $('avg').textContent=s.samples?'Average '+fmt(s.avgFreq,0)+' Hz (sd '+fmt(s.sdFreq,0)+') over '+s.samples+' readings, '+fmt(s.litres,3)+' L in '+s.durationS+' s → '+s.verdict:'';
}

async function post(url,body){const r=await fetch(url,{method:'POST',body:new URLSearchParams(body||{})});return r.json()}
$('calBtn').onclick=()=>post('/api/calibrate');
$('bench').onchange=e=>post('/api/settings',{bench:e.target.checked?1:0});
$('reset').onclick=()=>post('/api/session/reset');
$('defaults').onclick=async()=>{if(confirm('Restore factory thresholds?')){await post('/api/settings',{defaults:1});dirty=false}};
$('thForm').oninput=()=>{dirty=true};
$('thForm').onsubmit=async e=>{e.preventDefault();const r=await post('/api/settings',Object.fromEntries(new FormData(e.target)));
  $('formMsg').textContent=r.ok?'Saved.':'Not saved: '+r.error;if(r.ok)dirty=false};
poll();
</script></body></html>)QFLO";

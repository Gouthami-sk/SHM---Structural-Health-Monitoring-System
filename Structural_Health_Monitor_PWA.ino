#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "HX711.h"
#include <DHT.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ---------- WiFi ----------
const char* ssid = "****";
const char* password = "*****";

// ---------- Pins ----------
#define DHTPIN 4
#define VIB_PIN 27

#define NODE1_DT 32
#define NODE1_SCK 33

#define NODE2_DT 25
#define NODE2_SCK 26

#define MPU_SDA 21
#define MPU_SCL 22

// Set false if testing with only one load cell
#define USE_NODE1_LOADCELL true
#define USE_NODE2_LOADCELL true

// ---------- Sensors ----------
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
HX711 scale1;
HX711 scale2;
Adafruit_MPU6050 mpu;
WebServer server(80);

// ---------- Calibration ----------
float calibration_factor_1 = -7050.0;
float calibration_factor_2 = -7050.0;

// ---------- Thresholds ----------
float LOAD_WARNING_N = 30.0;
float LOAD_DANGER_N = 50.0;
float LOAD_IMBALANCE_N = 25.0;
float TILT_WARNING_DEG = 10.0;
float TILT_DANGER_DEG = 18.0;
unsigned long VIB_DANGER_TIME = 2000;

// ---------- Values ----------
float node1Load = 0.0;
float node2Load = 0.0;
float temperatureC = 0.0;
float humidity = 0.0;
float tiltDeg = 0.0;

bool node1LoadOK = false;
bool node2LoadOK = false;
bool dhtOK = false;
bool mpuOK = false;

int vibrationValue = 0;
bool vibrationDanger = false;
unsigned long vibrationStart = 0;

int node1FaultCount = 0;
int node2FaultCount = 0;
int dhtFaultCount = 0;

// ---------- Local Email Demo ----------
const int MAX_EMAILS = 10;
String registeredEmails[MAX_EMAILS];
int emailCount = 0;

String mainAlert = "System starting...";
String alertPreview = "No danger alert required right now.";

// ---------- Mobile App HTML ----------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Structural Monitor</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="theme-color" content="#f6f7f2">
<link rel="manifest" href="/manifest.json">
<style>
:root{
  --bg:#f6f7f2;--card:#ffffff;--ink:#111915;--muted:#68736b;--line:#dfe5dc;
  --blue:#2f6f8f;--green:#257a45;--amber:#a96b13;--red:#b42318;--purple:#5b3fd6;
}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);font-family:Arial,Helvetica,sans-serif}
.app{max-width:520px;margin:auto;min-height:100vh;padding:18px 14px 34px}
.top{display:flex;justify-content:space-between;gap:14px;align-items:flex-start;margin-bottom:18px}
h1{font-size:2.45rem;line-height:.92;margin:0;font-weight:900;letter-spacing:-1px}
.pill{border:1px solid var(--line);background:white;border-radius:999px;padding:8px 12px;color:var(--muted);font-weight:800;font-size:.78rem;white-space:nowrap}
.block{background:white;border:1px solid var(--line);border-radius:18px;padding:14px;margin-bottom:12px}
label{display:block;color:var(--muted);font-weight:900;font-size:.78rem;margin-bottom:8px;text-transform:uppercase}
select,input{width:100%;padding:12px;border:1px solid var(--line);border-radius:10px;background:white;color:var(--ink);font-size:1rem}
.hero{padding:16px}
.heroRow{display:flex;justify-content:space-between;gap:12px;align-items:flex-start}
.place{font-size:1.55rem;font-weight:900;margin:0}
.kind{color:var(--muted);margin-top:5px;font-size:.92rem}
.state{font-weight:900;border-radius:999px;padding:7px 11px;border:1px solid var(--line);font-size:.82rem;background:white}
.normal{color:var(--green)}.warning{color:var(--amber)}.danger{color:var(--red)}.waiting{color:var(--muted)}
.metrics{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:12px}
.metric{background:white;border:1px solid var(--line);border-radius:16px;padding:13px;min-height:108px}
.metricTitle{font-weight:900;color:var(--muted);font-size:.88rem;margin-bottom:20px}
.value{font-size:1.48rem;font-weight:900;line-height:1}
.note{color:var(--muted);font-size:.84rem;margin-top:9px;line-height:1.28}
h2{font-size:1.05rem;margin:0 0 10px}
.msg{white-space:pre-wrap;color:var(--muted);line-height:1.42;font-size:.94rem}
.register{display:grid;grid-template-columns:1fr auto;gap:8px}
button{border:0;border-radius:10px;background:var(--blue);color:white;font-weight:900;padding:0 14px;font-size:.95rem}
.small{font-size:.84rem;color:var(--muted);line-height:1.35;margin-top:10px}
.links{display:flex;gap:8px;flex-wrap:wrap;margin-top:10px}
.linkBtn{display:inline-block;text-decoration:none;color:var(--blue);font-weight:900;border:1px solid var(--line);background:white;border-radius:999px;padding:8px 11px;font-size:.86rem}
.graphHead{display:flex;justify-content:space-between;align-items:center;gap:10px;margin-bottom:10px}
.graphHint{font-size:.82rem;color:var(--muted);font-weight:800}
.swipe{
  display:flex;
  overflow-x:auto;
  gap:12px;
  scroll-snap-type:x mandatory;
  -webkit-overflow-scrolling:touch;
  padding-bottom:8px;
  margin:0 -2px 12px;
}
.swipe::-webkit-scrollbar{height:5px}
.swipe::-webkit-scrollbar-thumb{background:#cfd8cf;border-radius:999px}
.graphCard{
  min-width:88%;
  scroll-snap-align:start;
  background:white;
  border:1px solid var(--line);
  border-radius:18px;
  padding:14px;
}
.graphTitle{font-weight:900;margin-bottom:4px}
.graphSub{font-size:.84rem;color:var(--muted);margin-bottom:10px}
canvas{width:100%;height:210px;display:block}
.legend{display:flex;gap:12px;flex-wrap:wrap;color:var(--muted);font-size:.82rem;margin-top:8px}
.dot{display:inline-block;width:9px;height:9px;border-radius:999px;margin-right:5px}
@media(max-width:380px){
  h1{font-size:2rem}.metrics{grid-template-columns:1fr}.register{grid-template-columns:1fr}
  button{padding:12px}.graphCard{min-width:94%}
}
</style>
</head>
<body>
<div class="app">
  <div class="top">
    <h1>Structural Health Monitor</h1>
    <div id="conn" class="pill">Waiting</div>
  </div>

  <div class="block">
    <label>Monitoring location</label>
    <select id="placeSelect" onchange="switchPlace()">
      <option value="krs">KRS Dam - Live ESP32</option>
      <option value="mandya">Mandya Bridge - Demo</option>
      <option value="mysuru">Mysuru Flyover - Demo</option>
      <option value="cauvery">Cauvery Check Dam - Demo</option>
      <option value="krishna">Krishna River Bridge - Demo</option>
    </select>
    <div class="links">
      <a class="linkBtn" href="/qr">QR code</a>
    </div>
  </div>

  <div class="block hero">
    <div class="heroRow">
      <div>
        <p id="placeName" class="place">KRS Dam</p>
        <div id="placeKind" class="kind">Live ESP32 structural readings</div>
      </div>
      <div id="overall" class="state waiting">Waiting</div>
    </div>
  </div>

  <div class="metrics">
    <div class="metric"><div class="metricTitle">Node 1 Load</div><div class="value" id="n1">--</div><div class="note" id="n1Note">No data yet</div></div>
    <div class="metric"><div class="metricTitle">Node 2 Load</div><div class="value" id="n2">--</div><div class="note" id="n2Note">No data yet</div></div>
    <div class="metric"><div class="metricTitle">Vibration</div><div class="value" id="vib">--</div><div class="note" id="vibNote">No data yet</div></div>
    <div class="metric"><div class="metricTitle">Tilt</div><div class="value" id="tilt">--</div><div class="note" id="tiltNote">No data yet</div></div>
    <div class="metric"><div class="metricTitle">Temperature</div><div class="value" id="temp">--</div><div class="note">DHT11 reading</div></div>
    <div class="metric"><div class="metricTitle">Humidity</div><div class="value" id="hum">--</div><div class="note">DHT11 reading</div></div>
  </div>

  <div class="graphHead">
    <h2>Sensor graphs</h2>
    <div class="graphHint">Swipe sideways</div>
  </div>

  <div class="swipe">
    <div class="graphCard">
      <div class="graphTitle">Node 1 Load</div>
      <div class="graphSub">Live load cell trend</div>
      <canvas id="gN1"></canvas>
      <div class="legend"><span><span class="dot" style="background:#2f6f8f"></span>Node 1</span></div>
    </div>

    <div class="graphCard">
      <div class="graphTitle">Node 2 Load</div>
      <div class="graphSub">Second load cell trend</div>
      <canvas id="gN2"></canvas>
      <div class="legend"><span><span class="dot" style="background:#8a5a12"></span>Node 2</span></div>
    </div>

    <div class="graphCard">
      <div class="graphTitle">Vibration</div>
      <div class="graphSub">SW420 wobble events</div>
      <canvas id="gVib"></canvas>
      <div class="legend"><span><span class="dot" style="background:#b42318"></span>Vibration</span></div>
    </div>

    <div class="graphCard">
      <div class="graphTitle">Tilt</div>
      <div class="graphSub">MPU6050 leg angle</div>
      <canvas id="gTilt"></canvas>
      <div class="legend"><span><span class="dot" style="background:#5b3fd6"></span>Tilt</span></div>
    </div>

    <div class="graphCard">
      <div class="graphTitle">Environment</div>
      <div class="graphSub">Temperature and humidity</div>
      <canvas id="gEnv"></canvas>
      <div class="legend">
        <span><span class="dot" style="background:#c05621"></span>Temp</span>
        <span><span class="dot" style="background:#257a45"></span>Humidity</span>
      </div>
    </div>
  </div>

  <div class="block">
    <h2>Node failure notifications</h2>
    <div id="faults" class="msg">No faults detected yet.</div>
  </div>

  <div class="block">
    <h2>Public safety alerts</h2>
    <div class="register">
      <input id="email" type="email" placeholder="email@example.com">
      <button onclick="registerEmail()">Register</button>
    </div>
    <div class="small">Demo only. Email IDs are stored locally on the ESP32. Real email sending is not enabled.</div>
    <br>
    <div id="emails" class="msg">No email IDs registered yet.</div>
  </div>

  <div class="block">
    <h2>Alert message preview</h2>
    <div id="preview" class="msg">No danger alert required right now.</div>
  </div>
</div>

<script>
const MAX=50;
let selected='krs';
let n1Hist=Array(MAX).fill(null),n2Hist=Array(MAX).fill(null),vibHist=Array(MAX).fill(null),tiltHist=Array(MAX).fill(null),tempHist=Array(MAX).fill(null),humHist=Array(MAX).fill(null);

const demoPlaces={
  mandya:{name:'Mandya Bridge',kind:'Simulated road bridge',base1:18,base2:21,temp:30,hum:58,tilt:4},
  mysuru:{name:'Mysuru Flyover',kind:'Simulated urban flyover',base1:25,base2:22,temp:29,hum:54,tilt:6},
  cauvery:{name:'Cauvery Check Dam',kind:'Simulated dam gate section',base1:32,base2:35,temp:28,hum:66,tilt:5},
  krishna:{name:'Krishna River Bridge',kind:'Simulated long-span bridge',base1:15,base2:17,temp:31,hum:49,tilt:3}
};

function push(a,v){a.push(v);a.shift();}
function setOverall(x){
  const el=document.getElementById('overall');
  if(x===2){el.innerText='Danger';el.className='state danger';}
  else if(x===1){el.innerText='Warning';el.className='state warning';}
  else if(x===3){el.innerText='Waiting';el.className='state waiting';}
  else{el.innerText='Normal';el.className='state normal';}
}
function switchPlace(){
  selected=document.getElementById('placeSelect').value;
  n1Hist.fill(null);n2Hist.fill(null);vibHist.fill(null);tiltHist.fill(null);tempHist.fill(null);humHist.fill(null);
  update();
}
function fakeData(key){
  const p=demoPlaces[key],t=Date.now()/1000;
  let n1=p.base1+Math.sin(t/3)*5+(Math.random()*2-1);
  let n2=p.base2+Math.cos(t/4)*5+(Math.random()*2-1);
  let vib=Math.random()>0.9?1:0;
  let tilt=p.tilt+Math.sin(t/5)*2;
  if(key==='cauvery'&&Math.floor(t)%18>12){n1=54;n2=39;}
  if(key==='mysuru'&&Math.floor(t)%20>15){tilt=19;}
  if(key==='krishna'&&Math.floor(t)%16>12){vib=1;}
  let overall=0,faults='No faults detected. Simulated structure is operating normally.',preview='No danger alert required right now.';
  if(n1>=50||n2>=50||tilt>=18||vib===1){
    overall=2;faults='DEMO DANGER: This simulated location is showing a structural risk condition. Check overload, tilt, or vibration.';
    preview='PUBLIC STRUCTURAL SAFETY ALERT\n\n'+faults+'\n\nLocation: '+p.name;
  }else if(n1>=30||n2>=30||tilt>=10){
    overall=1;faults='DEMO WARNING: Readings are above normal observation limits.';
  }
  return {site:p.name,kind:p.kind,n1Load:n1,n2Load:n2,n1OK:true,n2OK:true,n1Disabled:false,n2Disabled:false,vib:vib,vibDanger:vib===1,temp:p.temp+Math.sin(t/7),hum:p.hum+Math.cos(t/6)*4,dhtOK:true,mpuOK:true,tilt:tilt,overall:overall,alert:faults,wouldSend:preview};
}
function drawSingle(id,arr,color){
  const c=document.getElementById(id),ctx=c.getContext('2d');
  c.width=c.offsetWidth;c.height=c.offsetHeight;
  ctx.clearRect(0,0,c.width,c.height);
  ctx.strokeStyle='#dfe5dc';ctx.lineWidth=1;
  ctx.beginPath();ctx.moveTo(34,8);ctx.lineTo(34,c.height-24);ctx.lineTo(c.width-8,c.height-24);ctx.stroke();
  const vals=arr.filter(v=>v!==null).map(v=>Math.abs(v));
  const max=Math.max(1,...vals)*1.2;
  ctx.strokeStyle=color;ctx.lineWidth=2.8;ctx.beginPath();
  let started=false;
  arr.forEach((v,i)=>{
    if(v===null)return;
    const x=34+(i/(MAX-1))*(c.width-44);
    const y=(c.height-24)-((Math.abs(v)/max)*(c.height-38));
    if(!started){ctx.moveTo(x,y);started=true;}else ctx.lineTo(x,y);
  });
  ctx.stroke();
}
function drawEnv(){
  const c=document.getElementById('gEnv'),ctx=c.getContext('2d');
  c.width=c.offsetWidth;c.height=c.offsetHeight;
  ctx.clearRect(0,0,c.width,c.height);
  ctx.strokeStyle='#dfe5dc';ctx.lineWidth=1;
  ctx.beginPath();ctx.moveTo(34,8);ctx.lineTo(34,c.height-24);ctx.lineTo(c.width-8,c.height-24);ctx.stroke();
  function plot(arr,color){
    ctx.strokeStyle=color;ctx.lineWidth=2.6;ctx.beginPath();let started=false;
    arr.forEach((v,i)=>{
      if(v===null)return;
      const x=34+(i/(MAX-1))*(c.width-44);
      const y=(c.height-24)-((Math.abs(v)/100)*(c.height-38));
      if(!started){ctx.moveTo(x,y);started=true;}else ctx.lineTo(x,y);
    });
    ctx.stroke();
  }
  plot(tempHist,'#c05621');plot(humHist,'#257a45');
}
function drawAll(){
  drawSingle('gN1',n1Hist,'#2f6f8f');
  drawSingle('gN2',n2Hist,'#8a5a12');
  drawSingle('gVib',vibHist,'#b42318');
  drawSingle('gTilt',tiltHist,'#5b3fd6');
  drawEnv();
}
function applyData(d){
  document.getElementById('placeName').innerText=d.site||'KRS Dam';
  document.getElementById('placeKind').innerText=d.kind||'Live ESP32 structural readings';
  document.getElementById('n1').innerText=d.n1OK?d.n1Load.toFixed(1)+' N':'--';
  document.getElementById('n2').innerText=d.n2OK?d.n2Load.toFixed(1)+' N':'--';
  document.getElementById('n1Note').innerText=d.n1OK?'Live reading':(d.n1Disabled?'Disabled':'Sensor warning');
  document.getElementById('n2Note').innerText=d.n2OK?'Live reading':(d.n2Disabled?'Disabled':'Sensor warning');
  document.getElementById('vib').innerText=d.vib?'Yes':'No';
  document.getElementById('vibNote').innerText=d.vibDanger?'Continuous vibration':'SW420 live';
  document.getElementById('tilt').innerText=d.mpuOK?d.tilt.toFixed(1)+' deg':'--';
  document.getElementById('tiltNote').innerText=d.mpuOK?'MPU6050 live':'Sensor warning';
  document.getElementById('temp').innerText=d.dhtOK?d.temp.toFixed(1)+' C':'--';
  document.getElementById('hum').innerText=d.dhtOK?d.hum.toFixed(0)+' %':'--';
  document.getElementById('faults').innerText=d.alert||'No faults detected yet.';
  document.getElementById('preview').innerText=d.wouldSend||'No danger alert required right now.';
  setOverall(d.overall);
  push(n1Hist,d.n1OK?d.n1Load:null);push(n2Hist,d.n2OK?d.n2Load:null);push(vibHist,d.vib?1:0);push(tiltHist,d.mpuOK?d.tilt:null);push(tempHist,d.dhtOK?d.temp:null);push(humHist,d.dhtOK?d.hum:null);
  drawAll();
}
function update(){
  if(selected==='krs'){
    fetch('/data').then(r=>r.json()).then(d=>{
      document.getElementById('conn').innerText='Connected';
      d.site='KRS Dam';d.kind='Live ESP32 structural readings';
      applyData(d);
    }).catch(()=>{document.getElementById('conn').innerText='Waiting';setOverall(3);});
  }else{
    document.getElementById('conn').innerText='Demo';
    applyData(fakeData(selected));
  }
}
function updateEmails(){fetch('/emails').then(r=>r.text()).then(t=>document.getElementById('emails').innerText=t);}
function registerEmail(){
  const email=document.getElementById('email').value;
  fetch('/register?email='+encodeURIComponent(email)).then(r=>r.text()).then(t=>{alert(t);document.getElementById('email').value='';updateEmails();});
}
setInterval(update,1000);
update();updateEmails();
if('serviceWorker' in navigator){navigator.serviceWorker.register('/sw.js').catch(()=>{});}
</script>
</body>
</html>
)rawliteral";

const char qr_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Open Mobile App</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body{margin:0;background:#f6f7f2;color:#111915;font-family:Arial,Helvetica,sans-serif}
.box{max-width:460px;margin:auto;padding:28px 18px;text-align:center}
h1{font-size:2.3rem;line-height:1;margin:0 0 14px;font-weight:900}
.card{background:white;border:1px solid #dfe5dc;border-radius:18px;padding:18px}
img{width:240px;height:240px;max-width:100%}
p{color:#68736b;line-height:1.4}
a{color:#2f6f8f;font-weight:900}
</style>
</head>
<body>
<div class="box">
<h1>Scan to open</h1>
<div class="card">
<img id="qr" alt="QR code">
<p id="url"></p>
<p>Your phone must be on the same WiFi as the ESP32. After opening it, tap Add to Home Screen to use it like an app.</p>
<a href="/">Open dashboard</a>
</div>
</div>
<script>
const u=location.origin+'/';
document.getElementById('url').innerText=u;
document.getElementById('qr').src='https://api.qrserver.com/v1/create-qr-code/?size=240x240&data='+encodeURIComponent(u);
</script>
</body>
</html>
)rawliteral";


// ---------- Helpers ----------
String jsonEscape(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", "\\n");
  s.replace("\r", "");
  return s;
}

bool isEmailValid(String email) {
  email.trim();
  return email.length() >= 5 && email.indexOf('@') > 0 && email.indexOf('.') > 0;
}

String getUptimeText() {
  unsigned long totalSeconds = millis() / 1000;
  unsigned long hours = totalSeconds / 3600;
  unsigned long minutes = (totalSeconds % 3600) / 60;
  unsigned long seconds = totalSeconds % 60;

  String text = "";

  if (hours > 0) {
    text += String(hours) + "h ";
  }

  if (minutes > 0 || hours > 0) {
    text += String(minutes) + "m ";
  }

  text += String(seconds) + "s";

  return text;
}

int getOverallState() {
  if (millis() < 5000) return 3; // Waiting

  // Danger only for real structural danger
  if (node1LoadOK && abs(node1Load) >= LOAD_DANGER_N) return 2;
  if (node2LoadOK && abs(node2Load) >= LOAD_DANGER_N) return 2;
  if (vibrationDanger) return 2;
  if (mpuOK && tiltDeg >= TILT_DANGER_DEG) return 2;

  // Warning only for real structural early-warning signs
  if (node1LoadOK && abs(node1Load) >= LOAD_WARNING_N) return 1;
  if (node2LoadOK && abs(node2Load) >= LOAD_WARNING_N) return 1;
  if (node1LoadOK && node2LoadOK && abs(node1Load - node2Load) >= LOAD_IMBALANCE_N) return 1;
  if (vibrationValue == 1) return 1;
  if (mpuOK && tiltDeg >= TILT_WARNING_DEG) return 1;

  return 0; // Normal
}

// ---------- Alerts ----------
void updateAlertMessage() {
  int overall = getOverallState();

  String alert = "";
  String preview = "";

  if (overall == 3) {
    alert = "System is starting. Waiting for stable readings.";
    preview = "No public safety warning is required right now.";
  } else {
    if (!USE_NODE1_LOADCELL) {
      alert += "NODE 1 LOAD CELL: Disabled for testing.\n";
    } else if (!node1LoadOK) {
      alert += "NODE 1 SENSOR NOTICE: Load cell / HX711 is not giving stable readings. Check DT GPIO 32, SCK GPIO 33, VCC, GND, and wiring.\n";
    }

    if (!USE_NODE2_LOADCELL) {
      alert += "NODE 2 LOAD CELL: Disabled for testing.\n";
    } else if (!node2LoadOK) {
      alert += "NODE 2 SENSOR NOTICE: Load cell / HX711 is not giving stable readings. Check DT GPIO 25, SCK GPIO 26, VCC, GND, and wiring.\n";
    }

    if (!mpuOK) {
      alert += "NODE 2 SENSOR NOTICE: MPU6050 is not detected. Check SDA GPIO 21, SCL GPIO 22, VCC, and GND.\n";
    }

    if (!dhtOK) {
      alert += "ENVIRONMENT SENSOR NOTICE: DHT11 temperature/humidity readings are unavailable.\n";
    }

    if (node1LoadOK && abs(node1Load) >= LOAD_DANGER_N) {
      alert += "NODE 1 STRUCTURAL DANGER: Load crossed danger threshold. Bridge may be overloaded near Node 1.\n";
    } else if (node1LoadOK && abs(node1Load) >= LOAD_WARNING_N) {
      alert += "NODE 1 LOAD WARNING: Load is above normal range.\n";
    }

    if (node2LoadOK && abs(node2Load) >= LOAD_DANGER_N) {
      alert += "NODE 2 STRUCTURAL DANGER: Load crossed danger threshold. Bridge may be overloaded near Node 2.\n";
    } else if (node2LoadOK && abs(node2Load) >= LOAD_WARNING_N) {
      alert += "NODE 2 LOAD WARNING: Load is above normal range.\n";
    }

    if (node1LoadOK && node2LoadOK && abs(node1Load - node2Load) >= LOAD_IMBALANCE_N) {
      alert += "LOAD IMBALANCE WARNING: Node 1 and Node 2 loads are uneven. This can indicate bending or uneven stress distribution.\n";
    }

    if (vibrationDanger) {
      alert += "NODE 1 VIBRATION DANGER: Continuous vibration detected. This may indicate loose joints or unstable support.\n";
    } else if (vibrationValue == 1) {
      alert += "NODE 1 VIBRATION WARNING: Short vibration event detected.\n";
    }

    if (mpuOK && tiltDeg >= TILT_DANGER_DEG) {
      alert += "NODE 2 TILT DANGER: Tilt crossed danger threshold. Support leg may be bending or giving way.\n";
    } else if (mpuOK && tiltDeg >= TILT_WARNING_DEG) {
      alert += "NODE 2 TILT WARNING: Tilt is higher than normal.\n";
    }

    if (alert == "") {
      alert = "No faults detected. KRS Dam live readings are within safe limits.";
    }

    if (overall == 2) {
      preview = "PUBLIC SAFETY WARNING\n\n";
      preview += "A possible structural safety problem has been detected at KRS Dam.\n";
      preview += "Detected time: " + getUptimeText() + " after system start.\n\n";
      preview += "Problem summary:\n";

      if (node1LoadOK && abs(node1Load) >= LOAD_DANGER_N) {
        preview += "- Dangerous stress has been detected near Node 1 of the structure.\n";
      }

      if (node2LoadOK && abs(node2Load) >= LOAD_DANGER_N) {
        preview += "- Dangerous stress has been detected near Node 2 of the structure.\n";
      }

      if (vibrationDanger) {
        preview += "- Continuous abnormal shaking has been detected.\n";
      }

      if (mpuOK && tiltDeg >= TILT_DANGER_DEG) {
        preview += "- Abnormal tilt has been detected in one support section.\n";
      }

      preview += "\nPeople nearby should avoid unnecessary movement near this structure and report the issue to the responsible authorities immediately.\n";
      preview += "This is an automated local warning generated by the structural monitoring system.";
    } else if (overall == 1) {
      preview = "PUBLIC CAUTION NOTICE\n\n";
      preview += "KRS Dam is showing early signs of unusual structural behaviour.\n";
      preview += "Detected time: " + getUptimeText() + " after system start.\n\n";
      preview += "People nearby should stay alert. No immediate danger alert has been triggered.";
    } else {
      preview = "No public safety warning is required right now.";
    }
  }

  mainAlert = alert;
  alertPreview = preview;
}



// ---------- Routes ----------
void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleQR() {
  server.send_P(200, "text/html", qr_html);
}

void handleManifest() {
  String manifest = "{";
  manifest += "\"name\":\"Structural Health Monitor\",";
  manifest += "\"short_name\":\"SHM\",";
  manifest += "\"start_url\":\"/\",";
  manifest += "\"display\":\"standalone\",";
  manifest += "\"background_color\":\"#f7f8f3\",";
  manifest += "\"theme_color\":\"#f7f8f3\",";
  manifest += "\"icons\":[]";
  manifest += "}";
  server.send(200, "application/manifest+json", manifest);
}

void handleSW() {
  String sw = "self.addEventListener('install',e=>self.skipWaiting());";
  sw += "self.addEventListener('fetch',e=>{});";
  server.send(200, "application/javascript", sw);
}

void handleRegister() {
  if (!server.hasArg("email")) {
    server.send(400, "text/plain", "No email received.");
    return;
  }

  String email = server.arg("email");
  email.trim();

  if (!isEmailValid(email)) {
    server.send(400, "text/plain", "Invalid email ID.");
    return;
  }

  for (int i = 0; i < emailCount; i++) {
    if (registeredEmails[i] == email) {
      server.send(200, "text/plain", "This email is already registered.");
      return;
    }
  }

  if (emailCount >= MAX_EMAILS) {
    server.send(200, "text/plain", "Email list is full. Restart ESP32 to clear it.");
    return;
  }

  registeredEmails[emailCount] = email;
  emailCount++;

  server.send(200, "text/plain", "Email registered locally. Real email sending is disabled.");
}

void handleEmails() {
  String response = "";
  if (emailCount == 0) response = "No email IDs registered yet.";
  else {
    response = "Registered email IDs:\n";
    for (int i = 0; i < emailCount; i++) response += String(i + 1) + ". " + registeredEmails[i] + "\n";
  }
  server.send(200, "text/plain", response);
}

void handleData() {
  updateAlertMessage();

  int overall = 0;
  bool waiting = millis() < 5000;

  if (waiting) {
    overall = 3;
  } else {
    // Danger only for actual structural danger
    if (node1LoadOK && abs(node1Load) >= LOAD_DANGER_N) overall = 2;
    if (node2LoadOK && abs(node2Load) >= LOAD_DANGER_N) overall = 2;
    if (vibrationDanger) overall = 2;
    if (mpuOK && tiltDeg >= TILT_DANGER_DEG) overall = 2;

    // Warning only for actual risky readings, not disconnected sensors
    int overall = getOverallState();
  }
  String json = "{";
  json += "\"n1Load\":" + String(node1Load, 2) + ",";
  json += "\"n2Load\":" + String(node2Load, 2) + ",";
  json += "\"n1OK\":" + String(node1LoadOK ? "true" : "false") + ",";
  json += "\"n2OK\":" + String(node2LoadOK ? "true" : "false") + ",";
  json += "\"n1Disabled\":" + String(USE_NODE1_LOADCELL ? "false" : "true") + ",";
  json += "\"n2Disabled\":" + String(USE_NODE2_LOADCELL ? "false" : "true") + ",";
  json += "\"vib\":" + String(vibrationValue) + ",";
  json += "\"vibDanger\":" + String(vibrationDanger ? "true" : "false") + ",";
  json += "\"temp\":" + String(temperatureC, 1) + ",";
  json += "\"hum\":" + String(humidity, 1) + ",";
  json += "\"dhtOK\":" + String(dhtOK ? "true" : "false") + ",";
  json += "\"mpuOK\":" + String(mpuOK ? "true" : "false") + ",";
  json += "\"tilt\":" + String(tiltDeg, 2) + ",";
  json += "\"overall\":" + String(overall) + ",";
  json += "\"alert\":\"" + jsonEscape(mainAlert) + "\",";
  json += "\"wouldSend\":\"" + jsonEscape(alertPreview) + "\"";
  json += "}";

  server.send(200, "application/json", json);
}


// ---------- Sensor Reads ----------
void readLoadCells() {
  static unsigned long lastRead = 0;
  if (millis() - lastRead < 250) return;
  lastRead = millis();

  if (USE_NODE1_LOADCELL) {
    if (scale1.wait_ready_timeout(80)) {
      float raw1 = scale1.get_units(2);
      node1Load = raw1 * 0.35 + node1Load * 0.65;
      if (abs(node1Load) < 0.20) node1Load = 0.0;
      node1FaultCount = 0;
      node1LoadOK = true;
    } else {
      node1FaultCount++;
      if (node1FaultCount > 25) node1LoadOK = false;
    }
  } else {
    node1LoadOK = false;
    node1Load = 0.0;
  }

  if (USE_NODE2_LOADCELL) {
    if (scale2.wait_ready_timeout(80)) {
      float raw2 = scale2.get_units(2);
      node2Load = raw2 * 0.35 + node2Load * 0.65;
      if (abs(node2Load) < 0.20) node2Load = 0.0;
      node2FaultCount = 0;
      node2LoadOK = true;
    } else {
      node2FaultCount++;
      if (node2FaultCount > 25) node2LoadOK = false;
    }
  } else {
    node2LoadOK = false;
    node2Load = 0.0;
  }
}

void readVibration() {
  vibrationValue = digitalRead(VIB_PIN);

  if (vibrationValue == 1) {
    if (vibrationStart == 0) vibrationStart = millis();
    if (millis() - vibrationStart > VIB_DANGER_TIME) vibrationDanger = true;
  } else {
    vibrationStart = 0;
    vibrationDanger = false;
  }
}

void readDHT11() {
  static unsigned long lastDHT = 0;
  if (millis() - lastDHT > 2000) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      temperatureC = t;
      humidity = h;
      dhtFaultCount = 0;
      dhtOK = true;
    } else {
      dhtFaultCount++;
      if (dhtFaultCount > 3) dhtOK = false;
    }

    lastDHT = millis();
  }
}

void readMPU6050() {
  if (!mpuOK) return;

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float ax = a.acceleration.x;
  float ay = a.acceleration.y;
  float az = a.acceleration.z;

  float pitch = atan2(ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
  float roll = atan2(ay, sqrt(ax * ax + az * az)) * 180.0 / PI;

  tiltDeg = max(abs(pitch), abs(roll));
}

void printSerial() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 2000) {
    Serial.print("KRS Dam | N1: ");
    Serial.print(node1Load, 2);
    Serial.print(" N | N2: ");
    Serial.print(node2Load, 2);
    Serial.print(" N | Vib: ");
    Serial.print(vibrationValue ? "YES" : "NO");
    Serial.print(" | Tilt: ");
    Serial.print(tiltDeg, 2);
    Serial.print(" deg | Temp: ");
    Serial.print(temperatureC, 1);
    Serial.print(" C | Hum: ");
    Serial.print(humidity, 1);
    Serial.print(" % | IP: ");
    Serial.println(WiFi.localIP());
    lastPrint = millis();
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Starting Structural Health Monitor Mobile App...");

  dht.begin();
  pinMode(VIB_PIN, INPUT);

  scale1.begin(NODE1_DT, NODE1_SCK);
  scale1.set_scale(calibration_factor_1);

  if (USE_NODE1_LOADCELL && scale1.wait_ready_timeout(2000)) {
    scale1.tare();
    node1LoadOK = true;
    Serial.println("Node 1 HX711 ready.");
  } else {
    node1LoadOK = false;
    node1FaultCount = 30;
    Serial.println("Node 1 HX711 not ready. Continuing.");
  }

  scale2.begin(NODE2_DT, NODE2_SCK);
  scale2.set_scale(calibration_factor_2);

  if (USE_NODE2_LOADCELL && scale2.wait_ready_timeout(2000)) {
    scale2.tare();
    node2LoadOK = true;
    Serial.println("Node 2 HX711 ready.");
  } else {
    node2LoadOK = false;
    node2FaultCount = 30;
    Serial.println("Node 2 HX711 not ready. Continuing.");
  }

  Wire.begin(MPU_SDA, MPU_SCL);

  if (mpu.begin()) {
    mpuOK = true;
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.println("MPU6050 detected.");
  } else {
    mpuOK = false;
    Serial.println("MPU6050 not detected. Continuing.");
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("--- SYSTEM READY ---");
  Serial.print("Mobile app: http://");
  Serial.println(WiFi.localIP());
  Serial.print("QR page: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/qr");

  server.on("/", handleRoot);
  server.on("/qr", handleQR);
  server.on("/data", handleData);
  server.on("/register", handleRegister);
  server.on("/emails", handleEmails);
  server.on("/manifest.json", handleManifest);
  server.on("/sw.js", handleSW);

  server.begin();
  Serial.println("Web server started.");
}

// ---------- Loop ----------
void loop() {
  server.handleClient();

  readLoadCells();
  readVibration();
  readDHT11();
  readMPU6050();
  printSerial();
}

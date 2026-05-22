#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "HX711.h"
#include <DHT.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ---------------- WiFi ----------------
const char* ssid = "StarLight";
const char* password = "Gouthami@1062";

// Paste your exact Google Apps Script Executable URL here
const String googleWebAppUrl = "https://script.google.com/macros/s/AKfycbx16xhyXgTG0vlrkFy0t7o76BkGRPw24aZ9Tv0BVdaC2xK5Q6OI7C85aLFQCBq_-kQx/exec";

// ---------------- Pins ----------------
#define DHTPIN 4
#define VIB_PIN 27
#define BUZZER_PIN 25  // <-- NEW: Connected to physical buzzer warning array

#define NODE1_DT 32
#define NODE1_SCK 33

#define MPU_SDA 21
#define MPU_SCL 22

// Extra pins for second HX711/load cell
#define NODE2_DT 25
#define NODE2_SCK 26

// Set false only if you are testing with one load cell temporarily
#define USE_NODE1_LOADCELL true
#define USE_NODE2_LOADCELL true

// ---------------- Sensors ----------------
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

HX711 scale1;
HX711 scale2;

Adafruit_MPU6050 mpu;
WebServer server(80);

// ---------------- Calibration ----------------
float calibration_factor_1 = -7050.0;
float calibration_factor_2 = -7050.0;

// ---------------- Thresholds ----------------
float LOAD_WARNING_N = 3.0;
float LOAD_DANGER_N = 5.0;
float LOAD_IMBALANCE_N = 25.0;

float TILT_WARNING_DEG = 90.0;
float TILT_DANGER_DEG = 95.0;

unsigned long VIB_DANGER_TIME = 2000;

// ---------------- Live Values ----------------
float node1Load = 0.0;
float node2Load = 0.0;

bool node1LoadOK = false;
bool node2LoadOK = false;
bool dhtOK = false;
bool mpuOK = false;

int vibrationValue = 0;
bool vibrationDanger = false;
unsigned long vibrationStart = 0;

float temperatureC = 0.0;
float humidity = 0.0;

float tiltDeg = 0.0;

// ---------------- Fault Counters ----------------
int node1FaultCount = 0;
int node2FaultCount = 0;
int dhtFaultCount = 0;

String mainAlert = "System starting...";
String lastWouldSendMessage = "No alert generated yet.";

// ---------------- Automation Safety Locks ----------------
bool autonomousEmailLocked = false; // <-- NEW: Prevents mail quota spamming on continuous faults

// ---------------- Dashboard HTML ----------------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Structural Health Monitor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    :root {
      --bg: #f7f8f5;
      --card: #ffffff;
      --ink: #14201a;
      --muted: #6d776f;
      --line: #dde3dc;
      --blue: #2f6f8f;
      --green: #2f7d46;
      --amber: #b7791f;
      --red: #b42318;
    }

    * { box-sizing: border-box; }

    body {
      margin: 0;
      background: var(--bg);
      color: var(--ink);
      font-family: Arial, Helvetica, sans-serif;
    }

    main {
      max-width: 1400px;
      margin: auto;
      padding: 22px 18px 40px;
    }

    .top {
      display: flex;
      justify-content: space-between;
      align-items: flex-start;
      gap: 16px;
      border-bottom: 1px solid var(--line);
      padding-bottom: 22px;
    }

    h1 {
      margin: 0;
      font-size: clamp(2rem, 4vw, 3.2rem);
      line-height: 0.95;
      letter-spacing: -0.02em;
      font-weight: 900;
    }

    .pill {
      border: 1px solid var(--line);
      background: white;
      border-radius: 999px;
      padding: 10px 16px;
      color: var(--muted);
      font-weight: 700;
      white-space: nowrap;
      margin-top: 4px;
    }

    .dashboard-layout {
      display: grid;
      grid-template-columns: 320px 1fr;
      gap: 24px;
      margin-top: 24px;
    }

    .control-sidebar {
      display: flex;
      flex-direction: column;
      gap: 20px;
    }

    .main-telemetry {
      display: flex;
      flex-direction: column;
      gap: 20px;
    }

    .alerts-sidebar-box {
      padding: 18px;
    }

    h2 {
      margin: 0 0 8px;
      font-size: 1.05rem;
    }

    p {
      margin: 0;
      color: var(--muted);
      font-size: 0.95rem;
      line-height: 1.4;
    }

    .register {
      display: flex;
      flex-direction: column;
      gap: 8px;
      margin-top: 14px;
    }

    input {
      border: 1px solid var(--line);
      border-radius: 6px;
      padding: 12px;
      font-size: 1rem;
      background: white;
      color: var(--ink);
      width: 100%;
    }

    button {
      border: none;
      border-radius: 6px;
      padding: 12px 16px;
      background: var(--blue);
      color: white;
      font-size: 1rem;
      font-weight: 800;
      cursor: pointer;
      width: 100%;
    }

    .cards {
      display: grid;
      grid-template-columns: repeat(5, 1fr);
      gap: 12px;
    }

    .card, .panel {
      background: var(--card);
      border: 1px solid var(--line);
      border-radius: 8px;
    }

    .card {
      min-height: 118px;
      padding: 16px;
    }

    .label {
      color: var(--muted);
      font-weight: 800;
      margin-bottom: 24px;
      font-size: 0.9rem;
      text-transform: uppercase;
      letter-spacing: 0.05em;
    }

    .value {
      font-size: 1.7rem;
      font-weight: 900;
      line-height: 1;
    }

    .note {
      color: var(--muted);
      margin-top: 12px;
      font-size: 0.85rem;
    }

    .notification {
      padding: 18px;
    }

    .status-title {
      display: flex;
      justify-content: space-between;
      gap: 12px;
      align-items: center;
      margin-bottom: 10px;
    }

    .state {
      font-weight: 900;
      border-radius: 999px;
      padding: 7px 12px;
      border: 1px solid var(--line);
      background: white;
    }

    .normal { color: var(--green); }
    .warning { color: var(--amber); }
    .danger { color: var(--red); }
    .waiting { color: var(--muted); }

    .message {
      white-space: pre-wrap;
      color: var(--muted);
      line-height: 1.45;
      font-size: 1rem;
    }

    .charts {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 12px;
    }

    .panel {
      padding: 18px;
    }

    canvas {
      width: 100%;
      height: 220px;
      display: block;
      margin-top: 14px;
    }

    .emails {
      margin-top: 10px;
      white-space: pre-wrap;
      color: var(--muted);
      font-size: 0.88rem;
    }

    /* Autonomous System Console Color Classes */
    .status-box {
      padding: 18px;
      transition: all 0.4s ease;
    }
    .status-normal { border: 1px solid var(--line); background: white; }
    .status-warning { border: 2px solid var(--amber); background: #fffbeb; }
    .status-danger { border: 2px solid var(--red); background: #fff5f5; }

    @media (max-width: 1100px) {
      .dashboard-layout { grid-template-columns: 1fr; }
      .control-sidebar { flex-direction: row; }
      .control-sidebar > div, .control-sidebar > section { flex: 1; margin-bottom: 0 !important; }
    }

    @media (max-width: 980px) {
      .control-sidebar { flex-direction: column; }
      .cards { grid-template-columns: repeat(2, 1fr); }
      .charts { grid-template-columns: 1fr; }
    }

    @media (max-width: 620px) {
      main { padding: 16px 12px 32px; }
      .top { display: block; }
      .pill { display: inline-block; margin-top: 14px; }
      .cards { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
<main>
  <section class="top">
    <h1>Structural Health Monitor</h1>
    <div id="connectPill" class="pill">Connected, waiting for ESP32</div>
  </section>

  <div class="dashboard-layout">
    
    <aside class="control-sidebar">
      
      <section id="statusConsoleBox" class="panel status-box status-normal">
        <h2 id="statusConsoleTitle" style="margin: 0 0 6px; font-weight: 900;">SYSTEM RESPONSE: ACTIVE</h2>
        <p id="statusConsoleDesc" style="font-size: 0.88rem; margin: 0;">Monitoring edge channels continuously. Edge system is executing autonomous surveillance parameters.</p>
      </section>

      <div class="panel alerts-sidebar-box">
        <h2>Public safety alerts</h2>
        <p>Register an email address to append data directly into the remote cloud logging spreadsheet subsystem.</p>
        
        <div class="register">
          <input id="emailInput" type="email" placeholder="email@example.com">
          <button onclick="registerEmail()">Register</button>
        </div>
        <div id="emails" class="emails">Database status active.</div>
      </div>

    </aside>

    <div class="main-telemetry">
      
      <section class="cards">
        <div class="card">
          <div class="label">Node 1 Load</div>
          <div class="value"><span id="n1Load">--</span></div>
          <div id="n1Note" class="note">No data yet</div>
        </div>

        <div class="card">
          <div class="label">Node 2 Load</div>
          <div class="value"><span id="n2Load">--</span></div>
          <div id="n2Note" class="note">No data yet</div>
        </div>

        <div class="card">
          <div class="label">Node 1 Vibration</div>
          <div class="value"><span id="vibValue">--</span></div>
          <div id="vibNote" class="note">No data yet</div>
        </div>

        <div class="card">
          <div class="label">Node 2 Tilt</div>
          <div class="value"><span id="tiltValue">--</span></div>
          <div id="tiltNote" class="note">No data yet</div>
        </div>

        <div class="card">
          <div class="label">Environment</div>
          <div class="value"><span id="envValue">--</span></div>
          <div id="envNote" class="note">No data yet</div>
        </div>
      </section>

      <section class="panel notification">
        <div class="status-title">
          <h2>Node failure notifications</h2>
          <div id="overallState" class="state waiting">Waiting</div>
        </div>
        <div id="status" class="message">No faults detected yet.</div>
      </section>

      <section class="charts">
        <div class="panel">
          <h2>Load cells</h2>
          <canvas id="loadCanvas"></canvas>
        </div>

        <div class="panel">
          <h2>Wobble sensors</h2>
          <canvas id="wobbleCanvas"></canvas>
        </div>

        <div class="panel">
          <h2>Environment</h2>
          <canvas id="envCanvas"></canvas>
        </div>
      </section>

    </div>
  </div>
</main>

<script>
  const MAX = 60;
  let n1 = Array(MAX).fill(null);
  let n2 = Array(MAX).fill(null);
  let vib = Array(MAX).fill(null);
  let tilt = Array(MAX).fill(null);
  let temp = Array(MAX).fill(null);
  let hum = Array(MAX).fill(null);

  function push(arr, value) {
    arr.push(value);
    arr.shift();
  }

  // Updates layout panel elements based on incoming background system structural state integers
  function setConsoleState(overall) {
    const el = document.getElementById('overallState');
    const box = document.getElementById('statusConsoleBox');
    const title = document.getElementById('statusConsoleTitle');
    const desc = document.getElementById('statusConsoleDesc');

    if (overall === 3) {
      el.innerText = 'Waiting';
      el.className = 'state waiting';
      box.className = 'panel status-box status-normal';
      title.innerText = "SYSTEM RESPONSE: INITIALIZING";
      title.style.color = "var(--ink)";
      desc.innerText = "Waiting for initial hardware handshake array payloads...";
      return;
    }

    if (overall === 2) {
      el.innerText = 'Danger';
      el.className = 'state danger';
      box.className = 'panel status-box status-danger';
      title.innerText = "CRITICAL HARDWARE FAULT";
      title.style.color = "var(--red)";
      desc.innerText = "Local structural thresholds broken. Local Audio Buzzer Beacon array has been activated continuously.";
    } else if (overall === 1) {
      el.innerText = 'Warning';
      el.className = 'state warning';
      box.className = 'panel status-box status-warning';
      title.innerText = "CLOUD DISPATCH WARNING";
      title.style.color = "var(--amber)";
      desc.innerText = "Anomalous stress profiles measured. Autonomous remote mailing relay active and dispatched.";
    } else {
      el.innerText = 'Normal';
      el.className = 'state normal';
      box.className = 'panel status-box status-normal';
      title.innerText = "SYSTEM RESPONSE: SAFE";
      title.style.color = "var(--green)";
      desc.innerText = "Monitoring edge channels continuously. Edge system is executing autonomous surveillance parameters.";
    }
  }

  function drawAxes(ctx, w, h) {
    ctx.clearRect(0, 0, w, h);
    ctx.strokeStyle = '#dde3dc';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(42, 8);
    ctx.lineTo(42, h - 28);
    ctx.lineTo(w - 8, h - 28);
    ctx.stroke();
    ctx.fillStyle = '#6d776f';
    ctx.font = '12px Arial';
    ctx.fillText('1.1', 8, 16);
  }

  function plot(canvasId, series) {
    const c = document.getElementById(canvasId);
    const ctx = c.getContext('2d');
    c.width = c.offsetWidth;
    c.height = c.offsetHeight;

    drawAxes(ctx, c.width, c.height);

    const vals = [];
    series.forEach(s => s.data.forEach(v => { if (v !== null) vals.push(Math.abs(v)); }));
    const max = Math.max(1, ...vals) * 1.15;

    series.forEach(s => {
      ctx.strokeStyle = s.color;
      ctx.lineWidth = 2.5;
      ctx.beginPath();
      let started = false;
      s.data.forEach((v, i) => {
        if (v === null) return;
        const x = 42 + (i / (MAX - 1)) * (c.width - 52);
        const y = (c.height - 28) - ((Math.abs(v) / max) * (c.height - 44));
        if (!started) {
          ctx.moveTo(x, y);
          started = true;
        } else {
          ctx.lineTo(x, y);
        }
      });
      ctx.stroke();
    });
  }

  function updateEmails() {
    fetch('/emails')
      .then(r => r.text())
      .then(t => document.getElementById('emails').innerText = t);
  }

  function registerEmail() {
    const email = document.getElementById('emailInput').value;
    fetch('/register?email=' + encodeURIComponent(email))
      .then(r => r.text())
      .then(t => {
        alert(t);
        document.getElementById('emailInput').value = '';
        updateEmails();
      });
  }

  function update() {
    fetch('/data')
      .then(r => r.json())
      .then(d => {
        document.getElementById('connectPill').innerText = 'Connected to ESP32';
        document.getElementById('n1Load').innerText = d.n1OK ? d.n1Load.toFixed(2) + ' N' : '--';
        document.getElementById('n2Load').innerText = d.n2OK ? d.n2Load.toFixed(2) + ' N' : '--';
        document.getElementById('n1Note').innerText = d.n1OK ? 'Live reading' : (d.n1Disabled ? 'Disabled for testing' : 'Sensor warning');
        document.getElementById('n2Note').innerText = d.n2OK ? 'Live reading' : (d.n2Disabled ? 'Disabled for testing' : 'Sensor warning');
        document.getElementById('vibValue').innerText = d.vib ? 'Yes' : 'No';
        document.getElementById('vibNote').innerText = d.vibDanger ? 'Continuous vibration' : 'SW420 live';
        document.getElementById('tiltValue').innerText = d.mpuOK ? d.tilt.toFixed(1) + ' deg' : '--';
        document.getElementById('tiltNote').innerText = d.mpuOK ? 'MPU6050 live' : 'Sensor warning';
        document.getElementById('envValue').innerText = d.dhtOK ? d.temp.toFixed(1) + ' C' : '--';
        document.getElementById('envNote').innerText = d.dhtOK ? d.hum.toFixed(1) + '% humidity' : 'Sensor warning';
        document.getElementById('status').innerText = d.alert || 'No faults detected yet.';
        
        setConsoleState(d.overall);

        push(n1, d.n1OK ? d.n1Load : null);
        push(n2, d.n2OK ? d.n2Load : null);
        push(vib, d.vib ? 1 : 0);
        push(tilt, d.mpuOK ? d.tilt : null);
        push(temp, d.dhtOK ? d.temp : null);
        push(hum, d.dhtOK ? d.hum : null);

        plot('loadCanvas', [{ data: n1, color: '#2f6f8f' }, { data: n2, color: '#8a5a12' }]);
        plot('wobbleCanvas', [{ data: vib, color: '#b42318' }, { data: tilt, color: '#5b3fd6' }]);
        plot('envCanvas', [{ data: temp, color: '#c05621' }, { data: hum, color: '#2f7d46' }]);
      })
      .catch(() => {
        document.getElementById('connectPill').innerText = 'Waiting for ESP32';
      });
  }

  setInterval(update, 1000);
  update();
  updateEmails();
</script>
</body>
</html>
)rawliteral";

// Modified: Completely back-end autonomous loop handler execution engine wrapper
void executeAutonomousCloudAlert() {
  updateAlertMessage();
  
  if (lastWouldSendMessage == "No danger alert required right now.") {
    return; // System is safe
  }

  WiFiClientSecure client;
  client.setInsecure(); 
  HTTPClient http;
  
  String fullUrl = googleWebAppUrl + "?action=broadcast&body=" + customUrlEncode(lastWouldSendMessage);
  Serial.println("AUTONOMOUS WARNING THRESHOLD BROKEN: Dispatched remote broadcast package payload...");

  if (http.begin(client, fullUrl)) {
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.println("Cloud Broadcast complete. Server response: " + http.getString());
    } else {
      Serial.println("Cloud Execution error encountered: " + http.errorToString(httpCode));
    }
    http.end();
  }
}

// ---------------- Utility ----------------
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

String customUrlEncode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}

// ---------------- Diagnosis ----------------
void updateAlertMessage() {
  String alert = "";
  String wouldSend = "";

  if (!USE_NODE1_LOADCELL) {
    alert += "NODE 1 LOAD CELL: Disabled in code for testing.\n";
  } else if (!node1LoadOK) {
    alert += "NODE 1 LOAD CELL CONNECTION ISSUE: Node 1 is not giving stable HX711 readings.\n";
  }

  if (!USE_NODE2_LOADCELL) {
    alert += "NODE 2 LOAD CELL: Disabled in code for testing.\n";
  } else if (!node2LoadOK) {
    alert += "NODE 2 LOAD CELL CONNECTION ISSUE: Node 2 is not giving stable HX711 readings.\n";
  }

  if (node1LoadOK && abs(node1Load) >= LOAD_DANGER_N) {
    alert += "NODE 1 STRUCTURAL DANGER: Load at Node 1 has crossed the danger threshold.\n";
  } else if (node1LoadOK && abs(node1Load) >= LOAD_WARNING_N) {
    alert += "NODE 1 WARNING: Load at Node 1 is above normal.\n";
  }

  if (node2LoadOK && abs(node2Load) >= LOAD_DANGER_N) {
    alert += "NODE 2 STRUCTURAL DANGER: Load at Node 2 has crossed the danger threshold.\n";
  } else if (node2LoadOK && abs(node2Load) >= LOAD_WARNING_N) {
    alert += "NODE 2 WARNING: Load at Node 2 is above normal.\n";
  }

  if (node1LoadOK && node2LoadOK && abs(node1Load - node2Load) >= LOAD_IMBALANCE_N) {
    alert += "LOAD IMBALANCE WARNING: Node 1 and Node 2 loads are very different.\n";
  }

  if (vibrationDanger) {
    alert += "NODE 1 LEG VIBRATION DANGER: Continuous vibration is detected.\n";
  } else if (vibrationValue == 1) {
    alert += "NODE 1 LEG VIBRATION WARNING: Short vibration event detected.\n";
  }

  if (!mpuOK) {
    alert += "NODE 2 MPU6050 CONNECTION ISSUE: MPU6050 is not detected.\n";
  } else if (tiltDeg >= TILT_DANGER_DEG) {
    alert += "NODE 2 LEG TILT DANGER: Tilt has crossed the danger threshold.\n";
  } else if (tiltDeg >= TILT_WARNING_DEG) {
    alert += "NODE 2 LEG TILT WARNING: Tilt is higher than normal.\n";
  }

  if (!dhtOK) {
    alert += "DHT11 ISSUE: Temperature and humidity readings are invalid.\n";
  }

  if (alert == "") {
    alert = "ALL SYSTEMS NORMAL: Readings are within safe limits.";
  }

  // Modified: Prepares alert bodies for both WARNING or DANGER states autonomously
  if (alert != "ALL SYSTEMS NORMAL: Readings are within safe limits.") {
    wouldSend = "PUBLIC AUTONOMOUS STRUCTURAL SAFETY ALERT\n\n" + alert;
    wouldSend += "\nRegistered recipients managed externally in Google Sheets Cloud Storage.";
  } else {
    wouldSend = "No danger alert required right now.";
  }

  mainAlert = alert;
  lastWouldSendMessage = wouldSend;
}

// ---------------- Web Routes ----------------
void handleRoot() {
  server.send_P(200, "text/html", index_html);
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

  WiFiClientSecure client;
  client.setInsecure(); 

  HTTPClient http;
  String fullUrl = googleWebAppUrl + "?email=" + customUrlEncode(email);
  
  Serial.println("Forwarding registration to Google Sheets...");

  if (http.begin(client, fullUrl)) {
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      String googleResponse = http.getString();
      server.send(200, "text/plain", googleResponse);
    } else {
      server.send(500, "text/plain", "Failed to reach Google Sheets. Error: " + http.errorToString(httpCode));
    }
    http.end();
  } else {
    server.send(500, "text/plain", "Unable to establish a secure connection.");
  }
}

void handleEmails() {
  server.send(200, "text/plain", "Connected to Sheet: Active cloud database storage system running.");
}

void handleData() {
  updateAlertMessage();

  int overall = 0; 
  bool waitingForStartup = millis() < 5000;

  if (waitingForStartup) {
    overall = 3;
  } else {
    if (node1LoadOK && abs(node1Load) >= LOAD_DANGER_N) overall = 2;
    if (node2LoadOK && abs(node2Load) >= LOAD_DANGER_N) overall = 2;
    if (vibrationDanger) overall = 2;
    if (mpuOK && tiltDeg >= TILT_DANGER_DEG) overall = 2;

    if (overall == 0) {
      if (USE_NODE1_LOADCELL && !node1LoadOK) overall = 1;
      if (USE_NODE2_LOADCELL && !node2LoadOK) overall = 1;
      if (!dhtOK) overall = 1;
      if (!mpuOK) overall = 1;

      if (node1LoadOK && abs(node1Load) >= LOAD_WARNING_N) overall = 1;
      if (node2LoadOK && abs(node2Load) >= LOAD_WARNING_N) overall = 1;
      if (node1LoadOK && node2LoadOK && abs(node1Load - node2Load) >= LOAD_IMBALANCE_N) overall = 1;
      if (vibrationValue == 1) overall = 1;
      if (mpuOK && tiltDeg >= TILT_WARNING_DEG) overall = 1;
    }
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
  json += "\"loadWarning\":" + String(LOAD_WARNING_N, 1) + ",";
  json += "\"loadDanger\":" + String(LOAD_DANGER_N, 1) + ",";
  json += "\"tiltWarning\":" + String(TILT_WARNING_DEG, 1) + ",";
  json += "\"tiltDanger\":" + String(TILT_DANGER_DEG, 1) + ",";
  json += "\"overall\":" + String(overall) + ",";
  json += "\"alert\":\"" + jsonEscape(mainAlert) + "\",";
  json += "\"wouldSend\":\"" + jsonEscape(lastWouldSendMessage) + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

// ---------------- Sensor Reading ----------------
void readLoadCells() {
  static unsigned long lastRead = 0;
  if (millis() - lastRead < 250) return;
  lastRead = millis();

  if (USE_NODE1_LOADCELL) {
    if (scale1.wait_ready_timeout(80)) {
      float raw1 = scale1.get_units(2);
      node1Load = (raw1 * 0.35) + (node1Load * 0.65);
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
      node2Load = (raw2 * 0.35) + (node2Load * 0.65);
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

void printSerialTable() {
  static unsigned long lastPrint = 0;

  if (millis() - lastPrint > 2000) {
    Serial.print("N1: ");
    Serial.print(node1Load, 2);
    Serial.print(" N ");
    Serial.print(node1LoadOK ? "OK" : "NOT READY");

    Serial.print(" | N2: ");
    Serial.print(node2Load, 2);
    Serial.print(" N ");
    Serial.print(node2LoadOK ? "OK" : "NOT READY");

    Serial.print(" | Vib: ");
    Serial.print(vibrationValue == 1 ? "YES" : "NO");

    Serial.print(" | Tilt: ");
    Serial.print(tiltDeg, 2);
    Serial.print(" deg");

    Serial.print(" | IP: ");
    Serial.println(WiFi.localIP());

    lastPrint = millis();
  }
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\nStarting Autonomous Structural Health Monitoring System...");

  dht.begin();
  pinMode(VIB_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);       // <-- NEW: Set buzzer pin mode output
  digitalWrite(BUZZER_PIN, LOW);     // Ensure silent at boot architecture

  scale1.begin(NODE1_DT, NODE1_SCK);
  scale1.set_scale(calibration_factor_1);

  if (USE_NODE1_LOADCELL && scale1.wait_ready_timeout(2000)) {
    scale1.tare();
    node1LoadOK = true;
    node1FaultCount = 0;
    Serial.println("Node 1 HX711 ready and tared.");
  } else {
    node1LoadOK = false;
    node1FaultCount = 30;
    Serial.println("Node 1 HX711 not ready at startup.");
  }

  scale2.begin(NODE2_DT, NODE2_SCK);
  scale2.set_scale(calibration_factor_2);

  if (USE_NODE2_LOADCELL && scale2.wait_ready_timeout(2000)) {
    scale2.tare();
    node2LoadOK = true;
    node2FaultCount = 0;
    Serial.println("Node 2 HX711 ready and tared.");
  } else {
    node2LoadOK = false;
    node2FaultCount = 30;
    Serial.println("Node 2 HX711 not ready at startup.");
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
    Serial.println("MPU6050 not detected.");
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n--- SYSTEM READY ---");
  Serial.print("Dashboard: http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/register", handleRegister);
  server.on("/emails", handleEmails);
  server.begin();

  Serial.println("Web server started.");
}

// ---------------- Main Loop ----------------
void loop() {
  server.handleClient();

  readLoadCells();
  readVibration();
  readDHT11();
  readMPU6050();

  // --- AUTONOMOUS HARDWARE ENGINE ---
  // Determine structural safety state flags locally inside background ticks
  int currentOverallState = 0;
  bool systemBooting = millis() < 5000;

  if (!systemBooting) {
    // Check Danger Levels First
    if ((node1LoadOK && abs(node1Load) >= LOAD_DANGER_N) || 
        (node2LoadOK && abs(node2Load) >= LOAD_DANGER_N) || 
        vibrationDanger || 
        (mpuOK && tiltDeg >= TILT_DANGER_DEG)) {
      currentOverallState = 2;
    } 
    // Check Warning Levels Second
    else if ((USE_NODE1_LOADCELL && !node1LoadOK) || 
             (USE_NODE2_LOADCELL && !node2LoadOK) || 
             !dhtOK || 
             !mpuOK || 
             (node1LoadOK && abs(node1Load) >= LOAD_WARNING_N) || 
             (node2LoadOK && abs(node2Load) >= LOAD_WARNING_N) || 
             (node1LoadOK && node2LoadOK && abs(node1Load - node2Load) >= LOAD_IMBALANCE_N) || 
             (vibrationValue == 1) || 
             (mpuOK && tiltDeg >= TILT_WARNING_DEG)) {
      currentOverallState = 1;
    }
  }

  // 1. Direct Edge Actuation Strategy (Buzzer triggers immediately on DANGER)
  if (currentOverallState == 2) {
    digitalWrite(BUZZER_PIN, HIGH); 
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // 2. Direct Cloud Dispatch Strategy (Mails autonomously when dropping into WARNING or DANGER)
  if (currentOverallState == 1 || currentOverallState == 2) {
    if (!autonomousEmailLocked) {
      executeAutonomousCloudAlert();
      autonomousEmailLocked = true; // Lock system to save email quotas
    }
  } else if (currentOverallState == 0) {
    autonomousEmailLocked = false; // Reset lock once variables return to complete safety
  }

  printSerialTable();
}
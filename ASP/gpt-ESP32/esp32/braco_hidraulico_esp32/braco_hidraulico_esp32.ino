/*
  Braco Hidraulico ESP32 - 3 motores CC + 2 TB6612FNG + 6 fins de curso
  ESP32 DevKit V1 / ESP32-WROOM-32

  O ESP32 cria uma rede Wi-Fi e hospeda o painel Web.
  Fins de curso: COM -> GND e NC -> GPIO, usando INPUT_PULLUP.
  NC fechado = LOW; acionado/fio aberto = HIGH.

  IMPORTANTE:
  - Motores usam fonte propria.
  - GND da fonte, ESP32 e drivers devem ser comuns.
  - Nao alimente motores pelo ESP32.
  - Use chave fisica para cortar VM dos motores.
*/

#include <WiFi.h>
#include <WebServer.h>

// ---------- Wi-Fi ----------
const char* AP_SSID = "BracoHidraulico";
const char* AP_PASSWORD = "12345678";
WebServer server(80);

// ---------- Hardware ----------
const uint8_t STBY_PIN = 23;

struct Axis {
  uint8_t pwmPin;
  uint8_t in1Pin;
  uint8_t in2Pin;
  uint8_t limitForwardPin;
  uint8_t limitReversePin;
  bool inverted;

  char state;                 // S, A, R
  uint8_t pwm;
  unsigned long lastCommand;
  unsigned long reverseUntil;
  bool armed;
};

Axis axes[3] = {
  {25, 26, 27, 32, 13, false, 'S', 0, 0, 0, false},
  {14, 16, 17, 21, 22, false, 'S', 0, 0, 0, false},
  {33, 18, 19,  4,  5, false, 'S', 0, 0, 0, false}
};

const unsigned long COMMAND_TIMEOUT_MS = 450;
const unsigned long REVERSAL_PAUSE_MS = 120;

// ---------- Painel HTML ----------
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Braço Hidráulico ESP32</title>
<style>
:root{font-family:Arial,sans-serif;color-scheme:dark}
*{box-sizing:border-box}
body{margin:0;background:#111827;color:#f9fafb}
header{padding:18px;text-align:center;background:#0f172a;position:sticky;top:0;z-index:2}
h1{margin:0;font-size:1.35rem} .sub{opacity:.75;margin-top:6px;font-size:.9rem}
main{max-width:1000px;margin:auto;padding:16px}
.status{display:flex;gap:10px;align-items:center;justify-content:center;margin-bottom:15px;flex-wrap:wrap}
.dot{width:12px;height:12px;border-radius:50%;background:#ef4444}.dot.ok{background:#22c55e}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:14px}
.card{background:#1f2937;border:1px solid #374151;border-radius:16px;padding:16px}
.card h2{text-align:center;margin:0 0 12px}
.readout{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin:10px 0}
.badge{padding:8px;border-radius:9px;background:#111827;text-align:center;font-size:.85rem}
.badge.limit{background:#7f1d1d}
label{display:block;margin:12px 0 5px}
input[type=range]{width:100%}
.controls{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:12px}
button{border:0;border-radius:12px;padding:18px 8px;font-size:1rem;font-weight:700;touch-action:none;user-select:none}
.forward{background:#16a34a;color:white}.reverse{background:#2563eb;color:white}
.stop{background:#dc2626;color:white;grid-column:1/-1}
#stopAll{width:100%;background:#b91c1c;color:#fff;font-size:1.2rem;margin-top:16px}
.note{font-size:.82rem;opacity:.75;text-align:center;margin-top:14px}
@media(max-width:760px){.grid{grid-template-columns:1fr}.card{padding:14px}}
</style>
</head>
<body>
<header>
  <h1>Braço Hidráulico ESP32</h1>
  <div class="sub">3 motores • 6 fins de curso • TB6612FNG</div>
</header>
<main>
  <div class="status"><span id="dot" class="dot"></span><span id="connection">Conectando...</span></div>
  <div id="cards" class="grid"></div>
  <button id="stopAll">PARAR TODOS</button>
  <div class="note">Mantenha Avançar/Recuar pressionado. Soltar o botão para o respectivo motor.</div>
</main>
<script>
const motors = [1,2,3];
const desired = {};
const lastTelemetry = {};
let emergency = false;

const cards = document.getElementById('cards');
motors.forEach(id => {
  desired[id] = 'S';
  cards.insertAdjacentHTML('beforeend', `
  <section class="card">
    <h2>Motor ${id}</h2>
    <div class="readout">
      <div id="state${id}" class="badge">Estado: --</div>
      <div id="pwmText${id}" class="badge">PWM: 35%</div>
      <div id="lf${id}" class="badge">Limite avanço: --</div>
      <div id="lr${id}" class="badge">Limite recuo: --</div>
    </div>
    <label>Potência: <strong id="pct${id}">35%</strong></label>
    <input id="pwm${id}" type="range" min="10" max="70" value="35">
    <div class="controls">
      <button class="forward" data-id="${id}" data-action="A">▲ Avançar</button>
      <button class="reverse" data-id="${id}" data-action="R">▼ Recuar</button>
      <button class="stop" data-id="${id}" data-action="S">Parar motor ${id}</button>
    </div>
  </section>`);
});

function pwmFromPercent(p){ return Math.round(26 + (p-10)*(178-26)/(70-10)); }

async function command(id, action){
  if(emergency && action !== 'S') return;
  const pct = +document.getElementById('pwm'+id).value;
  const pwm = pwmFromPercent(pct);
  desired[id] = action;
  try {
    await fetch(`/api/motor?id=${id}&acao=${action}&pwm=${pwm}`, {cache:'no-store'});
  } catch(e) {}
}

document.querySelectorAll('input[type=range]').forEach(el=>{
  el.addEventListener('input', ()=>{
    const id=el.id.replace('pwm','');
    document.getElementById('pct'+id).textContent=el.value+'%';
    document.getElementById('pwmText'+id).textContent='PWM: '+el.value+'%';
  });
});

document.querySelectorAll('button[data-id]').forEach(btn=>{
  const id=+btn.dataset.id, action=btn.dataset.action;
  if(action==='S'){
    btn.addEventListener('click', ()=>command(id,'S'));
  } else {
    const start=e=>{e.preventDefault(); emergency=false; command(id,action)};
    const stop=e=>{e.preventDefault(); command(id,'S')};
    btn.addEventListener('pointerdown', start);
    btn.addEventListener('pointerup', stop);
    btn.addEventListener('pointercancel', stop);
    btn.addEventListener('pointerleave', e=>{ if(e.buttons) stop(e); });
  }
});

document.getElementById('stopAll').addEventListener('click', async ()=>{
  emergency=true;
  motors.forEach(i=>desired[i]='S');
  try{await fetch('/api/stop',{cache:'no-store'})}catch(e){}
});

setInterval(()=>{
  motors.forEach(id=>{
    if(desired[id] !== 'S') command(id, desired[id]);
  });
},150);

async function telemetry(){
  try{
    const r=await fetch('/api/status',{cache:'no-store'});
    if(!r.ok) throw new Error();
    const d=await r.json();
    d.motors.forEach(m=>{
      lastTelemetry[m.id]=Date.now();
      document.getElementById('state'+m.id).textContent='Estado: '+m.state;
      const lf=document.getElementById('lf'+m.id), lr=document.getElementById('lr'+m.id);
      lf.textContent='Limite avanço: '+(m.forwardLimit?'ATIVO':'livre');
      lr.textContent='Limite recuo: '+(m.reverseLimit?'ATIVO':'livre');
      lf.classList.toggle('limit',m.forwardLimit);
      lr.classList.toggle('limit',m.reverseLimit);
    });
    document.getElementById('dot').classList.add('ok');
    document.getElementById('connection').textContent='ESP32 conectado';
  }catch(e){
    document.getElementById('dot').classList.remove('ok');
    document.getElementById('connection').textContent='Sem comunicação';
  }
}
setInterval(telemetry,300);
telemetry();

document.addEventListener('visibilitychange', ()=>{
  if(document.hidden) fetch('/api/stop',{keepalive:true}).catch(()=>{});
});
window.addEventListener('pagehide', ()=>fetch('/api/stop',{keepalive:true}).catch(()=>{}));
</script>
</body>
</html>
)rawliteral";

// ---------- Motor ----------
bool forwardLimit(uint8_t i) { return digitalRead(axes[i].limitForwardPin) == HIGH; }
bool reverseLimit(uint8_t i) { return digitalRead(axes[i].limitReversePin) == HIGH; }

void rawStop(uint8_t i) {
  digitalWrite(axes[i].in1Pin, LOW);
  digitalWrite(axes[i].in2Pin, LOW);
  analogWrite(axes[i].pwmPin, 0);
  axes[i].state = 'S';
  axes[i].pwm = 0;
}

void updateStandby() {
  bool active = false;
  for (uint8_t i=0;i<3;i++) if (axes[i].state != 'S') active = true;
  digitalWrite(STBY_PIN, active ? HIGH : LOW);
}

void stopMotor(uint8_t i, bool arm=true) {
  rawStop(i);
  if (arm) axes[i].armed = true;
  updateStandby();
}

void stopAll(bool arm=true) {
  for (uint8_t i=0;i<3;i++) {
    rawStop(i);
    axes[i].armed = arm;
  }
  digitalWrite(STBY_PIN, LOW);
}

void drive(uint8_t i, char action, uint8_t pwm) {
  if (i > 2) return;

  if (action == 'S') {
    stopMotor(i, true);
    axes[i].lastCommand = millis();
    return;
  }

  if (!axes[i].armed) return;

  if (action == 'A' && forwardLimit(i)) { stopMotor(i, true); return; }
  if (action == 'R' && reverseLimit(i)) { stopMotor(i, true); return; }

  unsigned long now = millis();

  // Pausa antes de inverter diretamente o sentido.
  if ((axes[i].state == 'A' && action == 'R') ||
      (axes[i].state == 'R' && action == 'A')) {
    rawStop(i);
    axes[i].reverseUntil = now + REVERSAL_PAUSE_MS;
    axes[i].lastCommand = now;
    updateStandby();
    return;
  }

  if (now < axes[i].reverseUntil) return;

  bool forwardElectrical = (action == 'A');
  if (axes[i].inverted) forwardElectrical = !forwardElectrical;

  digitalWrite(STBY_PIN, HIGH);
  digitalWrite(axes[i].in1Pin, forwardElectrical ? HIGH : LOW);
  digitalWrite(axes[i].in2Pin, forwardElectrical ? LOW : HIGH);
  analogWrite(axes[i].pwmPin, pwm);

  axes[i].state = action;
  axes[i].pwm = pwm;
  axes[i].lastCommand = now;
}

void safetyLoop() {
  unsigned long now = millis();

  for (uint8_t i=0;i<3;i++) {
    if (axes[i].state == 'A' && forwardLimit(i)) stopMotor(i, true);
    if (axes[i].state == 'R' && reverseLimit(i)) stopMotor(i, true);

    if (axes[i].state != 'S' && now - axes[i].lastCommand > COMMAND_TIMEOUT_MS) {
      rawStop(i);
      axes[i].armed = false; // exige S antes de voltar a mover
    }
  }
  updateStandby();
}

// ---------- HTTP ----------
void handleMotor() {
  if (!server.hasArg("id") || !server.hasArg("acao")) {
    stopAll(false);
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"parametros\"}");
    return;
  }

  int id = server.arg("id").toInt();
  String a = server.arg("acao");
  int pwm = server.hasArg("pwm") ? server.arg("pwm").toInt() : 89;

  if (id < 1 || id > 3 || a.length()!=1 ||
      (a[0]!='A' && a[0]!='R' && a[0]!='S')) {
    stopAll(false);
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"comando\"}");
    return;
  }

  pwm = constrain(pwm, 0, 255);
  drive(id-1, a[0], (uint8_t)pwm);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleStatus() {
  String json = "{\"motors\":[";
  for (uint8_t i=0;i<3;i++) {
    if(i) json += ",";
    json += "{\"id\":" + String(i+1);
    json += ",\"state\":\"" + String(axes[i].state) + "\"";
    json += ",\"pwm\":" + String(axes[i].pwm);
    json += ",\"forwardLimit\":" + String(forwardLimit(i) ? "true":"false");
    json += ",\"reverseLimit\":" + String(reverseLimit(i) ? "true":"false");
    json += ",\"armed\":" + String(axes[i].armed ? "true":"false") + "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  pinMode(STBY_PIN, OUTPUT);
  digitalWrite(STBY_PIN, LOW);

  for (uint8_t i=0;i<3;i++) {
    pinMode(axes[i].pwmPin, OUTPUT);
    pinMode(axes[i].in1Pin, OUTPUT);
    pinMode(axes[i].in2Pin, OUTPUT);
    pinMode(axes[i].limitForwardPin, INPUT_PULLUP);
    pinMode(axes[i].limitReversePin, INPUT_PULLUP);
    rawStop(i);
  }

  // Comeca desarmado: o painel envia S antes do primeiro movimento.
  stopAll(false);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  server.on("/", HTTP_GET, [](){ server.send_P(200, "text/html; charset=utf-8", INDEX_HTML); });
  server.on("/api/motor", HTTP_GET, handleMotor);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/stop", HTTP_GET, [](){ stopAll(true); server.send(200,"application/json","{\"ok\":true}"); });
  server.onNotFound([](){ server.send(404,"text/plain","404"); });
  server.begin();

  Serial.println();
  Serial.println("=== BRACO HIDRAULICO ESP32 ===");
  Serial.print("Wi-Fi: "); Serial.println(AP_SSID);
  Serial.print("Senha: "); Serial.println(AP_PASSWORD);
  Serial.print("Painel: http://"); Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();
  safetyLoop();
  delay(1);
}

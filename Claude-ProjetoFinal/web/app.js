'use strict';
// HM-10: serviço FFE0, característica UART FFE1. Três circuitos independentes.
const SERVICE = 0xffe0, CHARACTERISTIC = 0xffe1;
const HEARTBEAT_MS = 150, TELEMETRY_TIMEOUT_MS = 1600;
const $ = id => document.getElementById(id);
const encoder = new TextEncoder(), decoder = new TextDecoder();
const stateNames = {S: 'Parado', A: 'Avançando', R: 'Recuando'};
const axes = Array.from({length: 3}, (_, i) => ({id: i + 1, desired: 'S', pointer: null, key: null, seen: false, lastTelemetry: 0}));
let device = null, characteristic = null, ready = false, connecting = false;
let writing = false, revision = 0, linkEpoch = 0, rxBuffer = '', logLines = [];
for (const axis of axes) {
  const id = axis.id;
  const section = document.createElement('section');
  section.className = 'card'; section.setAttribute('aria-labelledby', `title${id}`);
  section.innerHTML = `<h2 id="title${id}">Motor ${id} • Circuito ${id}</h2>
    <label for="speed${id}">Potência PWM: <output id="speedValue${id}">35%</output></label>
    <input id="speed${id}" type="range" min="10" max="70" value="35" step="1">
    <div class="controls"><button id="advance${id}" class="motion advance" disabled>→ Avançar<br><small>Mantenha pressionado</small></button><button id="retreat${id}" class="motion retreat" disabled>← Recuar<br><small>Mantenha pressionado</small></button></div>
    <button id="stop${id}" class="stop" disabled>■ PARAR MOTOR ${id}</button>
    <p id="command${id}" role="status">Comando: parado</p>
    <div class="telemetry"><p>Estado informado pelo Arduino: <strong id="motorState${id}">—</strong></p><p>Limite de avanço: <strong id="limitA${id}">—</strong></p><p>Limite de recuo: <strong id="limitR${id}">—</strong></p></div>
    <p class="hint">PWM ajusta a potência, sem medir velocidade ou posição. Comece baixo.</p>`;
  $('motors').appendChild(section);
}
function log(message) {
  logLines.push(`${new Date().toLocaleTimeString('pt-BR')} • ${message}`);
  logLines = logLines.slice(-30); $('log').textContent = logLines.join('\n');
}
function canMove() { return ready && axes.every(a => a.seen) && !document.hidden; }
function render() {
  $('connect').disabled = connecting || ready || !navigator.bluetooth || !window.isSecureContext;
  $('disconnect').disabled = !ready; $('stopAll').disabled = !ready;
  for (const a of axes) {
    $('advance' + a.id).disabled = $('retreat' + a.id).disabled = !canMove();
    $('stop' + a.id).disabled = !ready;
    $('advance' + a.id).classList.toggle('active', a.desired === 'A');
    $('retreat' + a.id).classList.toggle('active', a.desired === 'R');
    $('command' + a.id).textContent = `Comando: ${stateNames[a.desired].toLowerCase()}`;
  }
}
function setDesired(a, command) {
  if (command !== 'S' && !canMove()) return;
  a.desired = command; revision++; render(); void transmit();
}
function stopAxis(a) { a.pointer = null; a.key = null; setDesired(a, 'S'); }
function stopAll() {
  for (const a of axes) { a.desired = 'S'; a.pointer = null; a.key = null; }
  revision++; render(); void transmit();
}
function resetLink(message) {
  const oldDevice = device; linkEpoch++;
  ready = false; connecting = false; characteristic = null; device = null; rxBuffer = ''; revision++;
  for (const a of axes) {
    a.desired = 'S'; a.pointer = null; a.key = null; a.seen = false;
    $('motorState' + a.id).textContent = 'Sem confirmação';
    $('limitA' + a.id).textContent = $('limitR' + a.id).textContent = '—';
  }
  $('connection').textContent = message; render();
  if (oldDevice?.gatt.connected) oldDevice.gatt.disconnect();
}
async function writeText(text) {
  const bytes = encoder.encode(text); // Cada mensagem tem menos de 20 bytes.
  if (characteristic.properties.write) await characteristic.writeValueWithResponse(bytes);
  else await characteristic.writeValueWithoutResponse(bytes);
}
// Uma escrita de cada vez, sem acumular movimentos antigos na fila.
// A revisão invalida o restante do lote se um botão mudar enquanto se envia.
async function transmit() {
  if (!ready || writing || !characteristic) return;
  writing = true;
  const epoch = linkEpoch;
  let sentRevision;
  try {
    do {
      sentRevision = revision;
      if (axes.every(a => a.desired === 'S')) await writeText('S\n');
      else {
        // Priorizar as paradas individuais antes dos comandos de movimento.
        const order = [...axes].sort((a, b) => Number(b.desired === 'S') - Number(a.desired === 'S'));
        for (const a of order) {
          if (!ready || epoch !== linkEpoch || revision !== sentRevision) break;
          const pwm = a.desired === 'S' ? 0 : Math.round(Number($('speed' + a.id).value) * 255 / 100);
          await writeText(`M:${a.id},${a.desired},${pwm}\n`);
        }
      }
    } while (ready && epoch === linkEpoch && revision !== sentRevision);
  } catch (error) {
    if (epoch === linkEpoch) {
      log(`Falha no envio: ${error.message}. O Arduino para pelo tempo limite.`);
      resetLink('Conexão interrompida');
    }
  } finally {
    writing = false;
    if (ready && epoch !== linkEpoch) void transmit();
  }
}
function receive(event) {
  const v = event.target.value;
  rxBuffer += decoder.decode(new Uint8Array(v.buffer, v.byteOffset, v.byteLength));
  if (rxBuffer.length > 512) rxBuffer = rxBuffer.slice(-256);
  let index;
  while ((index = rxBuffer.indexOf('\n')) !== -1) {
    const line = rxBuffer.slice(0, index).trim(); rxBuffer = rxBuffer.slice(index + 1);
    const m = /^T:([123]),([SAR]),([01]),([01])$/.exec(line);
    if (!m) continue;
    const a = axes[Number(m[1]) - 1];
    a.lastTelemetry = Date.now(); a.seen = true;
    $('motorState' + a.id).textContent = stateNames[m[2]];
    $('limitA' + a.id).textContent = m[3] === '1' ? 'Ativo / circuito aberto' : 'Livre';
    $('limitR' + a.id).textContent = m[4] === '1' ? 'Ativo / circuito aberto' : 'Livre';
    if ((a.desired === 'A' && m[3] === '1') || (a.desired === 'R' && m[4] === '1')) stopAxis(a);
    render();
  }
}
async function connect() {
  if (connecting || ready) return;
  connecting = true; render(); $('connection').textContent = 'Selecione o HM-10…';
  const epoch = ++linkEpoch;
  try {
    // Seleção manual também acha módulos que não anunciam o serviço FFE0.
    const chosen = await navigator.bluetooth.requestDevice({acceptAllDevices: true, optionalServices: [SERVICE]});
    if (epoch !== linkEpoch) return;
    device = chosen;
    device.addEventListener('gattserverdisconnected', () => {
      if (device === chosen) { log('Bluetooth desconectado; nenhuma retomada automática.'); resetLink('Desconectado'); }
    }, {once: true});
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(SERVICE);
    characteristic = await service.getCharacteristic(CHARACTERISTIC);
    if (!characteristic.properties.write && !characteristic.properties.writeWithoutResponse) throw new Error('FFE1 não aceita escrita');
    if (!characteristic.properties.notify && !characteristic.properties.indicate) throw new Error('FFE1 não fornece telemetria');
    characteristic.addEventListener('characteristicvaluechanged', receive);
    await characteristic.startNotifications();
    if (epoch !== linkEpoch) return;
    ready = true; connecting = false;
    for (const a of axes) { a.desired = 'S'; a.seen = false; a.lastTelemetry = Date.now(); }
    revision++; $('connection').textContent = `Conectado: ${device.name || 'HM-10'} • aguardando Arduino`;
    log('BLE conectado. Aguardando telemetria dos três motores.'); render(); await transmit();
  } catch (error) {
    if (epoch === linkEpoch) { log(`Conexão não concluída: ${error.message}`); resetLink('Desconectado'); }
  }
}
$('connect').addEventListener('click', connect);
$('disconnect').addEventListener('click', async () => { stopAll(); await transmit(); resetLink('Desconectado'); });
$('stopAll').addEventListener('click', stopAll);
for (const a of axes) {
  $('stop' + a.id).addEventListener('click', () => stopAxis(a));
  $('speed' + a.id).addEventListener('input', () => {
    $('speedValue' + a.id).value = `${$('speed' + a.id).value}%`;
    revision++; if (a.desired !== 'S') void transmit();
  });
  for (const [prefix, command] of [['advance', 'A'], ['retreat', 'R']]) {
    const button = $(prefix + a.id);
    button.addEventListener('contextmenu', e => e.preventDefault());
    button.addEventListener('pointerdown', e => {
      if (e.button !== 0 || a.pointer !== null || a.key !== null || button.disabled) return;
      e.preventDefault(); a.pointer = e.pointerId;
      button.setPointerCapture(e.pointerId); setDesired(a, command);
    });
    for (const name of ['pointerup', 'pointercancel', 'lostpointercapture']) {
      button.addEventListener(name, e => { if (e.pointerId === a.pointer) stopAxis(a); });
    }
    button.addEventListener('keydown', e => {
      if (![' ', 'Enter'].includes(e.key)) return;
      e.preventDefault();
      if (!e.repeat && a.pointer === null && a.key === null && !button.disabled) { a.key = e.key; setDesired(a, command); }
    });
    button.addEventListener('keyup', e => { if (e.key === a.key) { e.preventDefault(); stopAxis(a); } });
    // Soltar o foco com teclado interrompe o movimento; captura de toque tem seu próprio ciclo.
    button.addEventListener('blur', () => { if (a.key !== null) stopAxis(a); });
  }
}
window.addEventListener('blur', stopAll);
window.addEventListener('pagehide', stopAll);
document.addEventListener('visibilitychange', () => { if (document.hidden) stopAll(); });
window.addEventListener('keydown', e => { if (e.key === 'Escape') stopAll(); });
setInterval(() => {
  if (!ready) return;
  if (axes.some(a => Date.now() - a.lastTelemetry > TELEMETRY_TIMEOUT_MS)) {
    stopAll(); log('Telemetria incompleta. Verifique a montagem antes de reconectar.'); resetLink('Arduino sem resposta'); return;
  }
  if (axes.every(a => a.seen)) $('connection').textContent = `Conectado: ${device?.name || 'HM-10'} • 3 motores`;
  void transmit();
}, HEARTBEAT_MS);
$('support').textContent = !window.isSecureContext ? 'Abra por HTTPS ou por localhost no próprio computador.'
  : !navigator.bluetooth ? 'Web Bluetooth indisponível. Use Chrome compatível no Android ou no computador.'
  : 'Bluetooth BLE disponível. Conecte com os três mecanismos parados.';
render();

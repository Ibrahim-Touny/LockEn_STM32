'use strict';

const net       = require('net');
const http      = require('http');
const WebSocket = require('ws');

const TCP_PORT  = 3000;   /* ESP8266 connects here */
const HTTP_PORT = 8080;   /* browser connects here  */
const MAX_LOG   = 50;

let lcdState = { l0: '---', l1: '' };
let accessLog = [];
let tcpSocket  = null;
let wss        = null;
let pendingPwdCallback = null;

/* ══════════════════════════════════════════════════════════════════════
 *  TCP server — one persistent connection from the ESP8266
 * ══════════════════════════════════════════════════════════════════════ */
const tcpServer = net.createServer(socket => {
    console.log('[TCP] Safe connected from', socket.remoteAddress);
    tcpSocket = socket;
    broadcast({ type: 'connected', ok: true });

    let rxBuf = '';
    socket.on('data', chunk => {
        rxBuf += chunk.toString();
        const lines = rxBuf.split('\n');
        rxBuf = lines.pop();                    /* keep incomplete tail */
        lines.forEach(line => {
            line = line.trim();
            if (!line) return;
            try { handleSafeMessage(JSON.parse(line)); }
            catch (_) { /* ignore malformed frames */ }
        });
    });

    socket.on('close', () => {
        console.log('[TCP] Safe disconnected');
        tcpSocket = null;
        broadcast({ type: 'connected', ok: false });
    });

    socket.on('error', err => console.error('[TCP] Socket error:', err.message));
});

tcpServer.listen(TCP_PORT, '0.0.0.0', () =>
    console.log(`[TCP] Waiting for safe on port ${TCP_PORT}`));

function handleSafeMessage(msg) {
    switch (msg.t) {
        case 'lcd':
            lcdState = { l0: msg.l0 || '', l1: msg.l1 || '' };
            broadcast({ type: 'lcd', l0: lcdState.l0, l1: lcdState.l1 });
            break;

        case 'log': {
            const entry = {
                time:    new Date().toISOString(),
                ts:      msg.ts,
                event:   msg.ev,
                factors: msg.fx || '',
            };
            accessLog.unshift(entry);
            if (accessLog.length > MAX_LOG) accessLog.length = MAX_LOG;
            broadcast({ type: 'log', entry });
            break;
        }

        case 'pwd_result':
            if (pendingPwdCallback) {
                pendingPwdCallback(msg.ok === true);
                pendingPwdCallback = null;
            }
            break;
    }
}

function sendToSafe(obj) {
    if (!tcpSocket || tcpSocket.destroyed) return false;
    try { tcpSocket.write(JSON.stringify(obj) + '\n'); return true; }
    catch (_) { return false; }
}

function broadcast(obj) {
    if (!wss) return;
    const str = JSON.stringify(obj);
    wss.clients.forEach(c => { if (c.readyState === WebSocket.OPEN) c.send(str); });
}

/* ══════════════════════════════════════════════════════════════════════
 *  HTTP server — serves dashboard and REST endpoints
 * ══════════════════════════════════════════════════════════════════════ */
const httpServer = http.createServer((req, res) => {
    if (req.method === 'GET' && req.url === '/') {
        res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
        res.end(DASHBOARD_HTML);

    } else if (req.method === 'POST' && req.url === '/api/auth') {
        let body = '';
        req.on('data', d => { if (body.length < 256) body += d; });
        req.on('end', () => {
            let pin;
            try { pin = JSON.parse(body).pin; } catch (_) {}

            if (typeof pin !== 'string' || !/^\d{1,8}$/.test(pin)) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end('{"ok":false,"error":"invalid_pin"}');
                return;
            }
            if (!tcpSocket) {
                res.writeHead(503, { 'Content-Type': 'application/json' });
                res.end('{"ok":false,"error":"safe_disconnected"}');
                return;
            }
            if (pendingPwdCallback) {
                res.writeHead(429, { 'Content-Type': 'application/json' });
                res.end('{"ok":false,"error":"request_pending"}');
                return;
            }

            const timer = setTimeout(() => {
                pendingPwdCallback = null;
                res.writeHead(504, { 'Content-Type': 'application/json' });
                res.end('{"ok":false,"error":"timeout"}');
            }, 6000);

            pendingPwdCallback = ok => {
                clearTimeout(timer);
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ ok }));
            };

            sendToSafe({ t: 'pwd', pin });
        });

    } else if (req.method === 'GET' && req.url === '/api/state') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ lcd: lcdState, connected: !!tcpSocket }));

    } else {
        res.writeHead(404);
        res.end('Not Found');
    }
});

wss = new WebSocket.Server({ server: httpServer });
wss.on('connection', ws => {
    /* Send current state immediately on connect */
    ws.send(JSON.stringify({
        type: 'init',
        lcd: lcdState,
        log: accessLog,
        connected: !!tcpSocket,
    }));
});

httpServer.listen(HTTP_PORT, '0.0.0.0', () =>
    console.log(`[HTTP] Dashboard: http://localhost:${HTTP_PORT}`));

/* ══════════════════════════════════════════════════════════════════════
 *  Embedded dashboard HTML
 * ══════════════════════════════════════════════════════════════════════ */
const DASHBOARD_HTML = /* html */`<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>LockEn Safe</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',sans-serif;background:#1a1a2e;color:#e0e0e0;padding:24px;min-height:100vh;display:flex;flex-direction:column}
h1{color:#00d4ff;margin-bottom:10px;font-size:3rem;letter-spacing:3px;text-align:center}
.subtitle{color:#555;font-size:1.2rem;margin-bottom:20px;text-align:center}
.status-bar{display:flex;align-items:center;justify-content:center;gap:10px;margin-bottom:20px}
.badge{display:inline-block;padding:6px 20px;border-radius:12px;font-size:1rem;font-weight:bold;transition:all .3s}
.badge{display:inline-block;padding:4px 12px;border-radius:12px;font-size:.75rem;font-weight:bold;transition:all .3s}
.badge.online{background:#00b36b22;color:#00b36b;border:1px solid #00b36b}
.badge.offline{background:#ff444422;color:#ff4444;border:1px solid #ff4444}
.grid{display:grid;grid-template-columns:340px 1fr;gap:20px;width:100%;flex:1}
@media(max-width:700px){.grid{grid-template-columns:1fr}}
.card{background:#16213e;border-radius:10px;padding:20px;border:1px solid #0f3460;display:flex;flex-direction:column}
.card h2{font-size:.8rem;text-transform:uppercase;letter-spacing:1px;color:#666;margin-bottom:14px}
.lcd{background:#0b1a0b;border:2px solid #00aa00;border-radius:6px;padding:10px 14px;
     font-family:'Courier New',monospace;font-size:1.05rem;color:#00ee00;line-height:2}
.lcd-row{white-space:pre;min-height:1.5em}
.sep{border:none;border-top:1px solid #0f3460;margin:16px 0}
.pin-wrap{display:flex;flex-direction:column;gap:10px}
.pin-input{background:#0d1520;border:1px solid #0f3460;color:#e0e0e0;
           padding:10px 14px;border-radius:6px;font-size:1.2rem;letter-spacing:6px;
           width:100%;outline:none;transition:border-color .2s}
.pin-input:focus{border-color:#00d4ff}
.pin-btn{background:#0f3460;color:#00d4ff;border:1px solid #00d4ff;padding:10px;
         border-radius:6px;cursor:pointer;font-size:.9rem;font-weight:bold;transition:background .2s}
.pin-btn:hover{background:#00d4ff22}
.pin-btn:disabled{opacity:.4;cursor:default}
.pin-msg{font-size:.82rem;min-height:1.1em;transition:color .3s}
.pin-msg.ok{color:#00b36b}
.pin-msg.err{color:#ff4444}
.pin-msg.pending{color:#aaa}
table{width:100%;border-collapse:collapse;font-size:.8rem}
thead th{text-align:left;color:#555;font-weight:normal;padding:4px 8px;border-bottom:1px solid #0f3460}
tbody td{padding:6px 8px;border-bottom:1px solid #0f346033}
.GRANTED{color:#00b36b}
.TIMEOUT,.DENIED{color:#ff8844}
.TAMPER{color:#ff4444}
.empty-log{color:#444;font-size:.8rem;padding:12px 8px;font-style:italic}
</style>
</head>
<body>
<h1>LockEn Safe</h1>
<p class="subtitle">Remote dashboard</p>
<div class="status-bar">
  <span class="badge offline" id="conn-badge">Disconnected</span>
</div>

<div class="grid">
  <div class="card">
    <h2>LCD Display</h2>
    <div class="lcd">
      <div class="lcd-row" id="lcd0">                </div>
      <div class="lcd-row" id="lcd1">                </div>
    </div>

    <hr class="sep">

    <h2>Remote PIN</h2>
    <div class="pin-wrap">
      <input class="pin-input" id="pin-input" type="password"
             maxlength="8" placeholder="Enter PIN" inputmode="numeric"
             onkeydown="if(event.key==='Enter')submitPin()">
      <button class="pin-btn" id="pin-btn" onclick="submitPin()">Submit PIN</button>
      <div class="pin-msg" id="pin-msg"></div>
    </div>
  </div>

  <div class="card">
    <h2>Access Log</h2>
    <table>
      <thead><tr><th>Time</th><th>Event</th><th>Factors</th></tr></thead>
      <tbody id="log-body">
        <tr><td colspan="3" class="empty-log">No events yet</td></tr>
      </tbody>
    </table>
  </div>
</div>

<script>
const ws = new WebSocket('ws://' + location.host);

ws.onmessage = e => {
  const msg = JSON.parse(e.data);
  if      (msg.type === 'init')      { setConnected(msg.connected); setLcd(msg.lcd); msg.log.forEach(addRow); }
  else if (msg.type === 'lcd')       { setLcd(msg); }
  else if (msg.type === 'log')       { addRow(msg.entry); }
  else if (msg.type === 'connected') { setConnected(msg.ok); }
};

ws.onerror = () => setConnected(false);

function setConnected(ok) {
  const b = document.getElementById('conn-badge');
  b.textContent = ok ? 'Safe Connected' : 'Disconnected';
  b.className   = 'badge ' + (ok ? 'online' : 'offline');
}

function setLcd(lcd) {
  document.getElementById('lcd0').textContent = pad(lcd.l0);
  document.getElementById('lcd1').textContent = pad(lcd.l1);
}

function pad(s) {
  s = (s || '').substring(0, 16);
  return s + ' '.repeat(Math.max(0, 16 - s.length));
}

function addRow(e) {
  const tbody = document.getElementById('log-body');
  /* Remove placeholder row */
  if (tbody.querySelector('.empty-log')) tbody.innerHTML = '';
  const tr  = document.createElement('tr');
  const t   = new Date(e.time).toLocaleTimeString();
  const cls = e.event || '';
  tr.innerHTML =
    '<td>' + t + '</td>' +
    '<td class="' + cls + '">' + (e.event || '') + '</td>' +
    '<td>' + (e.factors || '') + '</td>';
  tbody.prepend(tr);
  while (tbody.rows.length > 50) tbody.deleteRow(-1);
}

async function submitPin() {
  const input = document.getElementById('pin-input');
  const btn   = document.getElementById('pin-btn');
  const msg   = document.getElementById('pin-msg');
  const pin   = input.value.trim();

  if (!pin)   { msg.textContent = 'Enter a PIN first.'; msg.className = 'pin-msg err'; return; }

  btn.disabled   = true;
  msg.textContent = 'Sending…';
  msg.className   = 'pin-msg pending';

  try {
    const r = await fetch('/api/auth', {
      method:  'POST',
      headers: { 'Content-Type': 'application/json' },
      body:    JSON.stringify({ pin }),
    });
    const d = await r.json();
    if (d.ok) {
      msg.textContent = 'PIN accepted — factor registered!';
      msg.className   = 'pin-msg ok';
    } else {
      const reason = d.error ? ' (' + d.error + ')' : '';
      msg.textContent = 'PIN rejected' + reason;
      msg.className   = 'pin-msg err';
    }
  } catch (_) {
    msg.textContent = 'Network error.';
    msg.className   = 'pin-msg err';
  }

  input.value  = '';
  btn.disabled = false;
}
</script>
</body>
</html>`;

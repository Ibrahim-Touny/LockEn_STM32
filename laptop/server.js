'use strict';

const net       = require('net');
const http      = require('http');
const fs        = require('fs');
const path      = require('path');
const WebSocket = require('ws');
const { spawn } = require('child_process');

const TCP_PORT  = 3000;   /* ESP8266 connects here */
const HTTP_PORT = 8080;   /* browser connects here  */
const MAX_LOG   = 50;
const PYTHON_EXE = fs.existsSync(path.join(__dirname, '..', '.venv', 'Scripts', 'python.exe'))
    ? path.join(__dirname, '..', '.venv', 'Scripts', 'python.exe')
    : 'python';

let lcdState = { l0: '---', l1: '' };
let accessLog = [];
let tcpSocket  = null;
let wss        = null;
let pendingPwdCallback = null;
let enrolling = false;
let scanning  = false;
let scanTimer = null;

/* ── Python face-recognition child process ── */
let pyProc      = null;
let pyBusy      = false;
let pyCallbacks = [];

function spawnPython() {
    pyProc = spawn(PYTHON_EXE, [path.join(__dirname, 'face_recog.py')]);
    let pyBuf = '';
    pyProc.stdout.on('data', chunk => {
        pyBuf += chunk.toString();
        const lines = pyBuf.split('\n');
        pyBuf = lines.pop();
        lines.forEach(line => {
            line = line.trim();
            if (!line) return;
            try {
                const msg = JSON.parse(line);
                const cb  = pyCallbacks.shift();
                if (cb) { pyBusy = false; cb(msg); }
            } catch (_) {}
        });
    });
    pyProc.stderr.on('data', d => console.error('[Python]', d.toString().trim()));
    pyProc.on('close', () => {
        console.log('[Python] Process exited — restarting in 2 s');
        pyProc = null;
        setTimeout(spawnPython, 2000);
    });
}

function pyRequest(obj, callback) {
    if (!pyProc || pyBusy) { callback({ result: 'busy' }); return; }
    pyBusy = true;
    pyCallbacks.push(callback);
    pyProc.stdin.write(JSON.stringify(obj) + '\n');
}

spawnPython();

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

        case 'enroll_reset':
            enrolling = true;
            scanning  = false;
            if (scanTimer) { clearTimeout(scanTimer); scanTimer = null; }
            broadcast({ type: 'face_status', status: 'enrolling' });
            pyRequest({ cmd: 'reset' }, () => {});
            console.log('[Face] Enrollment started');
            break;

        case 'face_scan':
            if (!enrolling) {
                scanning = true;
                if (scanTimer) clearTimeout(scanTimer);
                scanTimer = setTimeout(() => {
                    scanning = false;
                    broadcast({ type: 'face_status', status: 'idle' });
                }, 15000);
                broadcast({ type: 'face_status', status: 'scanning' });
                console.log('[Face] Scan window open (15 s)');
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

    } else if (req.method === 'GET' && req.url === '/logo.png') {
        const logoPath = path.join(__dirname, 'logo.png');
        fs.readFile(logoPath, (err, data) => {
            if (err) { res.writeHead(404); res.end('Not Found'); return; }
            res.writeHead(200, { 'Content-Type': 'image/png' });
            res.end(data);
        });

    } else if (req.method === 'POST' && req.url === '/api/face/frame') {
        const chunks = [];
        req.on('data', d => chunks.push(d));
        req.on('end', () => {
            res.writeHead(200);
            res.end('ok');

            if (!enrolling && !scanning) return; /* discard when idle */

            const imageB64 = Buffer.concat(chunks).toString('base64');
            const cmd = enrolling ? 'enroll' : 'recognize';

            pyRequest({ cmd, image: imageB64 }, result => {
                if (cmd === 'enroll' && result.result === 'enrolled') {
                    enrolling = false;
                    sendToSafe({ t: 'enroll_done' });
                    broadcast({ type: 'face_status', status: 'enrolled' });
                    console.log('[Face] Enrollment complete');
                } else if (cmd === 'recognize' && result.result === 'match') {
                    scanning = false;
                    if (scanTimer) { clearTimeout(scanTimer); scanTimer = null; }
                    sendToSafe({ t: 'face_ok' });
                    broadcast({ type: 'face_status', status: 'matched' });
                    console.log('[Face] Match — factor posted to safe');
                }
            });
        });

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
body{font-family:'Segoe UI',sans-serif;background:#7ec4d8;color:#0e0e1e;padding:24px;min-height:100vh;display:flex;flex-direction:column}
.logo{max-height:100px;max-width:360px;object-fit:contain;margin-bottom:4px;display:block;margin-left:auto;margin-right:auto}
.subtitle{color:#2e6070;font-size:1.2rem;margin-bottom:20px;text-align:center}
.status-bar{display:flex;align-items:center;justify-content:center;gap:10px;margin-bottom:20px}
.badge{display:inline-block;padding:6px 20px;border-radius:12px;font-size:.85rem;font-weight:bold;transition:all .3s}
.badge.online{background:#00995822;color:#005a38;border:1px solid #009958}
.badge.offline{background:#dd333318;color:#aa1a00;border:1px solid #dd3333}
.grid{display:grid;grid-template-columns:340px 1fr;gap:20px;width:100%;flex:1}
@media(max-width:700px){.grid{grid-template-columns:1fr}}
.card{background:#bde0ee;border-radius:10px;padding:20px;border:1px solid #6ab4cc;display:flex;flex-direction:column}
.card h2{font-size:.8rem;text-transform:uppercase;letter-spacing:1px;color:#2e6070;margin-bottom:14px}
.lcd{background:#0b1a0b;border:2px solid #00aa00;border-radius:6px;padding:10px 14px;
     font-family:'Courier New',monospace;font-size:1.05rem;color:#00ee00;line-height:2}
.lcd-row{white-space:pre;min-height:1.5em}
.sep{border:none;border-top:1px solid #6ab4cc;margin:16px 0}
.pin-wrap{display:flex;flex-direction:column;gap:10px}
.pin-input{background:#d0ecf5;border:1px solid #6ab4cc;color:#0e0e1e;
           padding:10px 14px;border-radius:6px;font-size:1.2rem;letter-spacing:6px;
           width:100%;outline:none;transition:border-color .2s}
.pin-input:focus{border-color:#2090b0}
.pin-btn{background:#2090b0;color:#fff;border:none;padding:10px;
         border-radius:6px;cursor:pointer;font-size:.9rem;font-weight:bold;transition:background .2s}
.pin-btn:hover{background:#1a7898}
.pin-btn:disabled{opacity:.4;cursor:default}
.pin-msg{font-size:.82rem;min-height:1.1em;transition:color .3s}
.pin-msg.ok{color:#005a38}
.pin-msg.err{color:#aa1a00}
.pin-msg.pending{color:#2e6070}
table{width:100%;border-collapse:collapse;font-size:.8rem}
thead th{text-align:left;color:#2e6070;font-weight:600;padding:4px 8px;border-bottom:1px solid #6ab4cc}
tbody td{padding:6px 8px;border-bottom:1px solid #8abcd0;color:#0e0e1e}
.GRANTED{color:#005a38}
.TIMEOUT,.DENIED{color:#a04800}
.TAMPER{color:#aa1a00}
.empty-log{color:#4a8aa0;font-size:.8rem;padding:12px 8px;font-style:italic}
</style>
</head>
<body>
<img src="/logo.png" alt="LockEn Safe" class="logo">
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

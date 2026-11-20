#!/usr/bin/env node
// Mock robots for developing the app without hardware.
//   node tools/mock-robot.mjs            -> Chiku on :8787, Jinu on :8788
// In the app add a robot with host "localhost:8787" (Android emulator: "10.0.2.2:8787").
// It implements the same HTTP API as the firmware (docs/API.md) with simulated sensors.
import http from 'node:http';

const MOODS = ['normal','happy','sad','sleepy','angry','surprised','dizzy','love','thinking','listening','jealous','sulk','shy','excited','scared','cry'];
const GAITS = ['home','walk','back','left','right','bend','shakeleg','moonwalk','dance','jump','wiggle','happy','excited','angry','scared','sulk','jealous','shy','love','sad','surprised','tiltl','tiltr'];

function makeRobot(kind, port) {
  const chiku = kind === 'chiku';
  const nServos = chiku ? 5 : 4;
  const t0 = Date.now();
  const r = {
    mode: 'auto', mute: false, mood: 'normal', brightness: 200, volume: 60, dist: 80, pads: 0, avoid: false, gait: 'home', busy: false,
    power: 0, screen: 0, lat: 47.73, lon: 7.55,
    servo: Array.from({ length: nServos }, () => ({ trim: 0, min: chiku && false ? 0 : 10, max: 170, inv: false, cur: 90 })),
    alarms: Array.from({ length: 8 }, () => ({ on: false, h: 7, m: 0, days: 127 })),
    timers: [0, 0, 0], desk: { paired: false, host: '' }, events: [], count: 0, log: [], fwlog: [], logCount: 0, snap: 0, led: null,
  };
  const push = (type, a = 0, b = 0) => { r.events.push({ n: ++r.count, t: Date.now() - t0, type, a, b }); if (r.events.length > 48) r.events.shift(); r.fwlog && fwlog(2, `[event] ${type} a=${a} b=${b}`); };
  const logCall = (m) => { r.log.push(m); if (r.log.length > 200) r.log.shift(); };
  // firmware-style log ring (same shape as GET /api/log)
  const fwlog = (l, msg) => { r.fwlog.push({ n: ++r.logCount, t: Date.now() - t0, l, m: msg }); if (r.fwlog.length > 100) r.fwlog.shift(); };
  fwlog(2, `[boot] ${kind} fw mock, reset reason 1, heap 180 KB`);
  fwlog(2, '[diag] display: ok'); fwlog(2, '[diag] audio: ok'); fwlog(2, '[diag] pad_expander: ok PCF8574 0x20');
  setInterval(() => { fwlog(2, `[heap] free 180 KB, min 170 KB`); }, 60000);

  // wandering distance + imu
  setInterval(() => { r.dist = Math.max(3, Math.min(250, Math.round(r.dist + (Math.random() - 0.5) * 20))); }, 300);

  const state = () => ({
    robot: kind, api: 1, fw: 'mock', caps: chiku ? ['screens','alarms','timers','weather','imu','cam','pan','battery','display_test','desk'] : ['display_test','rgb_led','desk'],
    name: kind, ip: `127.0.0.1:${port}`, ssid: 'MockWiFi', rssi: -55, uptime: Math.round((Date.now() - t0) / 1000), heap: 180000,
    desk: { paired: r.desk.paired, host: r.desk.host, ok: r.desk.paired, hv: r.desk.paired, h: 82.0, moving: false, dir: 0, armed: true, travel: false, src: 'display', err: '', age: 300 },
    mode: r.mode, mute: r.mute, mood: r.mood, brightness: r.brightness, volume: r.volume, dist: r.dist, pads: r.pads, padsOnline: true,
    avoid: r.avoid, gait: r.gait, busy: r.busy, power: r.power, audio: true, servo: r.servo,
    ...(chiku ? {
      screen: r.screen, batt: 83, charging: false, pca: true, psram: 7000000, lat: r.lat, lon: r.lon,
      imu: { ok: true, x: 0.02, y: 0.01, z: 1.0, tilt: 1.5 },
      cam: { alive: true, ip: '192.168.1.50', state: 'sleep', stream: 'http://192.168.1.50/stream', snap: r.snap },
      weather: { valid: true, temp: 12.4, min: 8, max: 15, text: 'Partly cloudy' },
      alarms: r.alarms, timers: r.timers,
    } : {}),
  });

  const body = (req) => new Promise((ok) => { let s = ''; req.on('data', (c) => (s += c)); req.on('end', () => ok(s)); });

  http.createServer(async (req, res) => {
    const u = new URL(req.url, 'http://x');
    const q = Object.fromEntries(u.searchParams);
    const send = (obj, code = 200) => { res.writeHead(code, { 'Content-Type': 'application/json' }); res.end(JSON.stringify(obj)); };
    const ok = (msg) => send(msg ? { ok: true, msg } : { ok: true });
    logCall(`${req.method} ${u.pathname}${u.search}`);
    if (!['/api/state','/api/events','/api/log'].includes(u.pathname)) fwlog(2, `[api] ${req.method} ${u.pathname} ${[...u.searchParams].map(([k, v]) => `${k}=${v}`).join(' ')}`.trim());
    const p = u.pathname;
    if (req.method === 'GET') {
      if (p === '/api/state') return send(state());
      if (p === '/api/events') { const since = +q.since || 0; return send({ next: r.count, events: r.events.filter((e) => e.n > since) }); }
      if (p === '/api/voice/list') return send(['hello','happy','excited','shy','love','surprised','angry','sulk','scared','sad','jealous', ...(chiku ? ['alarm','timer','goodmorning'] : [])]);
      if (p === '/api/log') { const since = +q.since || 0; return send({ next: r.logCount, lines: r.fwlog.filter((l) => l.n > since) }); }
      if (p === '/api/diag') return send({ items: [
        { name: 'display', ok: true, detail: chiku ? 'CO5300 466x466' : 'ST7789 320x172', live: false },
        { name: 'audio', ok: true, detail: 'I2S', live: false },
        { name: 'pad_expander', ok: true, detail: 'PCF8574 0x20', live: false },
        ...(chiku ? [{ name: 'servo_driver', ok: true, detail: 'PCA9685', live: false }, { name: 'imu_gravity', ok: true, detail: '1.00 g (expect 1.0)', live: true }, { name: 'rtc', ok: true, detail: 'PCF85063', live: false }, { name: 'rtc_time', ok: true, detail: 'time valid', live: true }, { name: 'cam_link', ok: true, detail: 'sleep 192.168.1.50', live: true }] : []),
        { name: 'sonar', ok: r.dist > 0, detail: `${r.dist} cm`, live: true }, { name: 'wifi', ok: true, detail: '-55 dBm', live: true } ], heap: 180000, uptime: Math.round((Date.now() - t0) / 1000) });
      if (p === '/api/desk') return send(state().desk);
      if (p === '/__log') return send(r.log);
      res.writeHead(404); return res.end('not found');
    }
    const raw = await body(req);
    switch (p) {
      case '/api/mode': if (!['auto','manual','explore'].includes(q.m)) { res.writeHead(400); return res.end('bad'); } r.mode = q.m; r.avoid = q.m === 'explore'; r.power = 0; fwlog(2, `[mode] -> ${q.m}`); return ok(q.m);
      case '/api/mute': r.mute = q.on === '1'; return ok();
      case '/api/mood': r.mood = MOODS[+q.m] ?? 'normal'; { const h = q.hold === undefined ? 5000 : +q.hold; if (h > 0) setTimeout(() => { r.mood = 'normal'; }, h); } return ok();
      case '/api/motion':
        if (q.stop) { r.gait = 'home'; r.busy = false; return ok(); }
        if (q.avoid !== undefined) { r.avoid = q.avoid === '1'; return ok(); }
        if (!GAITS.includes(q.g)) { res.writeHead(400); return res.end('unknown gait'); }
        r.gait = q.g; r.busy = true; fwlog(2, `[motion] start ${q.g} x${q.n}`); setTimeout(() => { r.busy = false; r.gait = 'home'; fwlog(2, `[motion] end ${q.g}`); }, 1500); return ok(q.g);
      case '/api/servo/set': { const s = r.servo[+q.ch]; if (s) s.cur = Math.max(0, Math.min(180, +q.deg)); return ok(`servo ${q.ch} -> ${q.deg}`); }
      case '/api/servo/release': return ok();
      case '/api/servo/test': { const s = r.servo[+q.ch]; if (s) s.cur = +q.deg; return ok(); }
      case '/api/say': return ok('clip');
      case '/api/beep': case '/api/melody': case '/api/audio/stop': case '/api/look': return ok();
      case '/api/desk/pair': if (!q.token || q.token.length < 8) { res.writeHead(400); return res.end('token (8+ characters) required'); } if (r.token && r.token !== q.token) { res.writeHead(401); return res.end('wrong token'); } r.token = q.token; r.desk = { paired: true, host: q.host || 'desk.local' }; fwlog(2, `[desk] paired with ${r.desk.host}`); return ok('paired');
      case '/api/desk/unpair': if (r.token !== q.token) { res.writeHead(401); return res.end('wrong token'); } r.token = undefined; r.desk = { paired: false, host: '' }; return ok('unpaired');
      case '/api/desk/cmd': if (!r.token || r.token !== q.token) { res.writeHead(401); return res.end('wrong token'); } if (!r.desk.paired) { res.writeHead(400); return res.end('desk not paired'); } fwlog(2, `[desk] -> ${q.op}`); return ok(q.op);
      case '/api/led': r.led = q.off ? null : [+q.r, +q.g, +q.b]; return ok();
      case '/api/screen': r.screen = +q.id; return ok();
      case '/api/display': return ok();
      case '/api/sim':
        if (q.pad !== undefined) { const bit = 1 << +q.pad; push('pad', +q.pad, +q.g || 1); r.pads |= bit; setTimeout(() => { r.pads &= ~bit; }, 300); }
        else push(q.ev === 'tap' ? 'imu_tap' : q.ev);
        return ok();
      case '/api/set': { const d = JSON.parse(raw || '{}'); for (const k of ['brightness','volume','lat','lon']) if (d[k] !== undefined) r[k] = d[k]; if (d.servo) d.servo.forEach((s, i) => Object.assign(r.servo[i], s)); if (d.alarms) r.alarms = d.alarms; push('settings'); return ok(); }
      case '/api/timer': r.timers[0] = +q.sec; return ok();
      case '/api/timer/cancel': r.timers[+q.slot] = 0; return ok();
      case '/api/cam': if (q.cmd === 'snap') r.snap = 40000 + Math.round(Math.random() * 9000); return ok();
      case '/api/weather/refresh': case '/api/alarm/test': case '/api/reboot': case '/api/wifi/reset': case '/api/voice': case '/update': return ok();
    }
    res.writeHead(404); res.end('not found');
  }).listen(port, '0.0.0.0', () => console.log(`mock ${kind} on http://localhost:${port}`));
}

makeRobot('chiku', 8787);
makeRobot('jinu', 8788);

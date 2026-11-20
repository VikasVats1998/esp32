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
    timers: [0, 0, 0], events: [], count: 0, log: [],
  };
  const push = (type, a = 0, b = 0) => { r.events.push({ n: ++r.count, t: Date.now() - t0, type, a, b }); if (r.events.length > 48) r.events.shift(); };
  const logCall = (m) => { r.log.push(m); if (r.log.length > 200) r.log.shift(); };

  // wandering distance + imu
  setInterval(() => { r.dist = Math.max(3, Math.min(250, Math.round(r.dist + (Math.random() - 0.5) * 20))); }, 300);

  const state = () => ({
    robot: kind, api: 1, fw: 'mock', caps: chiku ? ['screens','alarms','timers','weather','imu','cam','pan','battery','display_test'] : ['display_test','rgb_led'],
    name: kind, ip: `127.0.0.1:${port}`, ssid: 'MockWiFi', rssi: -55, uptime: Math.round((Date.now() - t0) / 1000), heap: 180000,
    mode: r.mode, mute: r.mute, mood: r.mood, brightness: r.brightness, volume: r.volume, dist: r.dist, pads: r.pads, padsOnline: true,
    avoid: r.avoid, gait: r.gait, busy: r.busy, power: r.power, audio: true, servo: r.servo,
    ...(chiku ? {
      screen: r.screen, batt: 83, charging: false, pca: true, psram: 7000000, lat: r.lat, lon: r.lon,
      imu: { ok: true, x: 0.02, y: 0.01, z: 1.0, tilt: 1.5 },
      cam: { alive: true, ip: '192.168.1.50', state: 'sleep', stream: 'http://192.168.1.50/stream' },
      weather: { valid: true, temp: 12.4, min: 8, max: 15, text: 'Partly cloudy' },
      alarms: r.alarms, timers: r.timers,
    } : {}),
  });

  const body = (req) => new Promise((ok) => { let s = ''; req.on('data', (c) => (s += c)); req.on('end', () => ok(s)); });

  http.createServer(async (req, res) => {
    const u = new URL(req.url, 'http://x');
    const q = Object.fromEntries(u.searchParams);
    const send = (obj, code = 200) => { res.writeHead(code, { 'Content-Type': 'application/json' }); res.end(JSON.stringify(obj)); };
    const ok = () => send({ ok: true });
    logCall(`${req.method} ${u.pathname}${u.search}`);
    const p = u.pathname;
    if (req.method === 'GET') {
      if (p === '/api/state') return send(state());
      if (p === '/api/events') { const since = +q.since || 0; return send({ next: r.count, events: r.events.filter((e) => e.n > since) }); }
      if (p === '/api/voice/list') return send(['hello','happy','excited','shy','love','surprised','angry','sulk','scared','sad','jealous', ...(chiku ? ['alarm','timer','goodmorning'] : [])]);
      if (p === '/__log') return send(r.log);
      res.writeHead(404); return res.end('not found');
    }
    const raw = await body(req);
    switch (p) {
      case '/api/mode': if (!['auto','manual','explore'].includes(q.m)) { res.writeHead(400); return res.end('bad'); } r.mode = q.m; r.avoid = q.m === 'explore'; r.power = 0; return ok();
      case '/api/mute': r.mute = q.on === '1'; return ok();
      case '/api/mood': r.mood = MOODS[+q.m] ?? 'normal'; { const h = q.hold === undefined ? 5000 : +q.hold; if (h > 0) setTimeout(() => { r.mood = 'normal'; }, h); } return ok();
      case '/api/motion':
        if (q.stop) { r.gait = 'home'; r.busy = false; return ok(); }
        if (q.avoid !== undefined) { r.avoid = q.avoid === '1'; return ok(); }
        if (!GAITS.includes(q.g)) { res.writeHead(400); return res.end('unknown gait'); }
        r.gait = q.g; r.busy = true; setTimeout(() => { r.busy = false; r.gait = 'home'; }, 1500); return ok();
      case '/api/servo/set': { const s = r.servo[+q.ch]; if (s) s.cur = Math.max(0, Math.min(180, +q.deg)); return ok(); }
      case '/api/servo/release': return ok();
      case '/api/servo/test': { const s = r.servo[+q.ch]; if (s) s.cur = +q.deg; return ok(); }
      case '/api/say': case '/api/beep': case '/api/melody': case '/api/audio/stop': case '/api/look': return ok();
      case '/api/screen': r.screen = +q.id; return ok();
      case '/api/display': return ok();
      case '/api/sim':
        if (q.pad !== undefined) { const bit = 1 << +q.pad; push('pad', +q.pad, +q.g || 1); r.pads |= bit; setTimeout(() => { r.pads &= ~bit; }, 300); }
        else push(q.ev === 'tap' ? 'imu_tap' : q.ev);
        return ok();
      case '/api/set': { const d = JSON.parse(raw || '{}'); for (const k of ['brightness','volume','lat','lon']) if (d[k] !== undefined) r[k] = d[k]; if (d.servo) d.servo.forEach((s, i) => Object.assign(r.servo[i], s)); if (d.alarms) r.alarms = d.alarms; push('settings'); return ok(); }
      case '/api/timer': r.timers[0] = +q.sec; return ok();
      case '/api/timer/cancel': r.timers[+q.slot] = 0; return ok();
      case '/api/cam': case '/api/weather/refresh': case '/api/alarm/test': case '/api/reboot': case '/api/wifi/reset': case '/api/voice': case '/update': return ok();
    }
    res.writeHead(404); res.end('not found');
  }).listen(port, '0.0.0.0', () => console.log(`mock ${kind} on http://localhost:${port}`));
}

makeRobot('chiku', 8787);
makeRobot('jinu', 8788);

// Mock desk module for developing the app without hardware. Speaks the real DeskLink v1 protocol
// (ECDH pairing, one-time challenges, AES-GCM) with the same crypto as the app.
//   npx tsx tools/mock-desk.mts            -> http://localhost:8790   (add the desk as "localhost:8790")
// Behaviour like the firmware: pairing window 180 s after start, a held UP/DOWN is released 450 ms after the last
// move message, 25 s run limit, child lock, 5 bad messages lock everything for 60 s.
import http from 'node:http';

import { deriveKey, fromHex, generateKeyPair, open, seal, toHex } from '../src/desk/crypto';

const PORT = Number(process.env.PORT ?? 8790);
const PAIRING_MS = Number(process.env.PAIRING_MS ?? 180_000);
const started = Date.now();
const rnd = (n: number) => crypto.getRandomValues(new Uint8Array(n));

interface Cl { id: string; name: string; key: Uint8Array }
const clients = new Map<string, Cl>();
const challenges = new Map<string, number>();
let fails: number[] = [];
let lockUntil = 0;

const desk = { h: 75.0, armed: true, phold: false, keys: { up: false, down: false } as Record<string, boolean>, releaseAt: 0, runStart: 0, travel: false, calibrated: true };
const log: { n: number; t: number; l: number; m: string }[] = [];
let ln = 0;
const L = (l: number, m: string) => { log.push({ n: ++ln, t: Date.now() - started, l, m }); if (log.length > 100) log.shift(); };
L(2, '[boot] desk module mock');

setInterval(() => {                                       // physics + dead-man
  const now = Date.now();
  if ((desk.keys.up || desk.keys.down) && now >= desk.releaseAt) { desk.keys.up = desk.keys.down = false; L(2, '[keys] released (keep-alive ended)'); }
  if ((desk.keys.up || desk.keys.down) && now - desk.runStart > 25_000) { desk.keys.up = desk.keys.down = false; L(1, '[keys] run limit reached'); }
  const dir = desk.keys.up ? 1 : desk.keys.down ? -1 : 0;
  desk.h = Math.min(125, Math.max(60, desk.h + dir * 3.6 * 0.05));
}, 50);

const pairingOpen = () => Date.now() - started < PAIRING_MS;
const state = () => ({
  ok: true, h: Math.round(desk.h * 10) / 10, hv: true, src: 'display', moving: desk.keys.up || desk.keys.down, dir: desk.keys.up ? 1 : desk.keys.down ? -1 : 0,
  travel: desk.travel, disp: desk.h.toFixed(1), armed: desk.armed, phold: desk.phold,
  keys: `${desk.keys.up ? '#' : '-'}${desk.keys.down ? '#' : '-'}------`, frames: 120, bad: 0, bytes: 1500, up: Math.round((Date.now() - started) / 1000), rssi: -50, fw: 'mock',
});

function handleOp(cl: Cl, q: Record<string, unknown>) {
  const op = String(q.op);
  const refuse = (err: string) => { L(1, `[link] ${op} refused: ${err}`); return { ok: false, err }; };
  switch (op) {
    case 'state': return state();
    case 'move': {
      if (!desk.armed) return refuse('disarmed (child lock is on)');
      if (q.dir !== 'up' && q.dir !== 'down') return refuse('dir must be up or down');
      const now = Date.now();
      const ms = Math.min(900, Math.max(40, Number(q.ms ?? 400)));
      if (!desk.keys.up && !desk.keys.down) { desk.runStart = now; L(2, `[keys] move ${q.dir}`); }
      desk.keys.up = q.dir === 'up'; desk.keys.down = q.dir === 'down'; desk.releaseAt = now + ms;
      return { ok: true };
    }
    case 'stop': desk.keys.up = desk.keys.down = false; desk.travel = false; L(2, '[keys] stop'); return { ok: true };
    case 'preset': if (!desk.armed) return refuse('disarmed (child lock is on)'); desk.travel = true; L(2, `[keys] go to preset ${q.n}`); setTimeout(() => { desk.travel = false; }, 1500); return { ok: true };
    case 'save': L(2, `[keys] save memory ${q.n}`); return { ok: true };
    case 'tap': L(2, `[keys] tap ${q.key}`); return { ok: true };
    case 'setheight': desk.h = Number(q.h); return { ok: true };
    case 'arm': desk.armed = q.on !== false; if (!desk.armed) desk.keys.up = desk.keys.down = false; return { ok: true };
    case 'cfg': if (typeof q.presetHold === 'boolean') desk.phold = q.presetHold; return { ok: true };
    case 'log': return { ok: true, next: ln, lines: log.filter((x) => x.n > Number(q.since ?? 0)) };
    case 'raw': return { ok: true, text: 'ok  F2 F2 01 03 03 3A 00 3E 7E\nok  F2 F2 12 03 7F 5B 3F 05 7E' };
    case 'clients': return { ok: true, clients: [...clients.values()].map((c) => ({ id: c.id, name: c.name, you: c.id === cl.id })) };
    case 'unpair': clients.delete(cl.id); return { ok: true };
    default: return refuse('unknown op');
  }
}

const body = (req: http.IncomingMessage) => new Promise<string>((ok) => { let s = ''; req.on('data', (c) => (s += c)); req.on('end', () => ok(s)); });

function authFailed(why: string) {
  const now = Date.now();
  fails = [...fails, now].filter((t) => now - t < 60_000);
  L(1, `[link] rejected message: ${why}`);
  if (fails.length >= 5) { lockUntil = now + 60_000; fails = []; L(0, '[link] too many bad messages: commands locked'); }
}

http.createServer(async (req, res) => {
  const u = new URL(req.url ?? '/', 'http://x');
  const send = (code: number, obj: unknown) => { res.writeHead(code, { 'Content-Type': typeof obj === 'string' ? 'text/plain' : 'application/json' }); res.end(typeof obj === 'string' ? obj : JSON.stringify(obj)); };
  if (req.method === 'GET' && u.pathname === '/dl/info') return send(200, { dev: 'desk', fw: 'mock', link: 1, pairing: pairingOpen(), left: Math.max(0, Math.round((PAIRING_MS - (Date.now() - started)) / 1000)), clients: clients.size, locked: Date.now() < lockUntil });
  if (req.method === 'GET' && u.pathname === '/dl/challenge') { const c = toHex(rnd(16)); challenges.set(c, Date.now()); return send(200, { c }); }
  if (req.method === 'POST' && u.pathname === '/dl/pair') {
    if (!pairingOpen()) return send(403, 'pairing is closed: power-cycle the desk to open it');
    if (clients.size >= 4) return send(507, 'all client slots used: unpair one');
    const d = JSON.parse(await body(req)) as { name: string; pub: string };
    let key: Uint8Array; let kp;
    try { kp = generateKeyPair(); key = deriveKey(kp, fromHex(d.pub), fromHex(d.pub), kp.pub); } catch { return send(400, 'bad public key'); }
    const id = toHex(rnd(4));
    clients.set(id, { id, name: d.name, key });
    L(2, `[link] paired ${d.name} as ${id}`);
    return send(200, { id, pub: toHex(kp.pub) });
  }
  if (req.method === 'POST' && u.pathname === '/dl/cmd') {
    if (Date.now() < lockUntil) return send(429, 'locked');
    const d = JSON.parse(await body(req)) as { id: string; c: string; d: string };
    const issued = challenges.get(d.c);
    challenges.delete(d.c);                                           // one use only
    if (!issued || Date.now() - issued > 10_000) { authFailed('unknown or expired challenge'); return send(401, 'auth'); }
    const cl = clients.get(d.id);
    let plain: string | undefined;
    try { if (cl) plain = open(cl.key, fromHex(d.c), 'cmd', fromHex(d.d)); } catch { /* bad tag */ }
    if (!cl || plain === undefined) { authFailed(cl ? 'bad tag' : 'unknown client'); return send(401, 'auth'); }
    const reply = handleOp(cl, JSON.parse(plain) as Record<string, unknown>);
    return send(200, { d: toHex(seal(cl.key, fromHex(d.c), 'rsp', JSON.stringify(reply))) });
  }
  send(404, 'not found');
}).listen(PORT, '0.0.0.0', () => console.log(`mock desk on http://localhost:${PORT} (pairing window ${PAIRING_MS / 1000} s)`));

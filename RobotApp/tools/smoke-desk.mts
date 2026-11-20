// Smoke test of the app's desk client against a desk module (the mock, or a real one).
//   npx tsx tools/mock-desk.mts &   then   npx tsx tools/smoke-desk.mts localhost:8790
import { DeskLinkClient, DeskError } from '../src/desk/client';
import { buildDeskTests } from '../src/lib/checklist';
import { deriveKey, fromHex, generateKeyPair, seal, toHex } from '../src/desk/crypto';

const host = process.argv[2] ?? 'localhost:8790';
let failed = 0;
const check = (name: string, ok: boolean, extra = '') => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${name} ${extra}`); if (!ok) failed++; };
const wait = (ms: number) => new Promise<void>((r) => setTimeout(r, ms));

const anon = new DeskLinkClient(host);
const info = await anon.info();
check('info (public)', info.dev === 'desk' && info.pairing, `fw ${info.fw}, ${info.clients} clients`);

// commands without pairing must fail
try { await anon.state(); check('no creds: refuse', false); } catch (e) { check('no creds: refuse', e instanceof DeskError); }

// pairing
const creds = await anon.pair('smoke test');
check('pairing gives id and 32 byte key', creds.id.length === 8 && creds.key.length === 64);
const desk = new DeskLinkClient(host, creds);
const s0 = await desk.state();
check('sealed state works', s0.ok && s0.hv && s0.armed, `${s0.h} cm`);

// movement + dead-man
await desk.move('up', 500);
await wait(120);
check('move up presses UP', (await desk.state()).keys[0] === '#');
await wait(900);
const s1 = await desk.state();
check('dead-man: released without keep-alive', s1.keys[0] === '-' && !s1.moving, s1.keys);
check('height rose', s1.h > s0.h, `${s0.h} -> ${s1.h}`);
await desk.move('down', 500);
await desk.stop();
check('stop releases', (await desk.state()).keys === '--------');

// child lock
await desk.arm(false);
const refused = await desk.move('up');
check('child lock refuses movement', refused.ok === false, String(refused.err));
await desk.arm(true);
check('move allowed again', (await desk.move('down', 100)).ok);
await desk.stop();

check('presets and keys', (await desk.preset(2)).ok && (await desk.tap('x', 100)).ok && (await desk.saveMemory(3)).ok);
check('calibrate height', (await desk.setHeight(80)).ok && Math.abs((await desk.state()).h - 80) < 1);
const lg = await desk.log(0);
check('desk log', lg.ok && lg.lines.length > 3, `${lg.lines.length} lines`);
check('clients list', (await desk.clients()).clients.some((c) => c.you));

// ---- the app's desk checklist (auto tests must pass, confirm tests must start without errors)
const ctx = { client: undefined as never, desk, kind: 'chiku' as const, note: () => {}, sleep: wait, errorsSince: () => 0 };
const lockoutRisk = process.env.TEST_LOCKOUT === '0';
for (const t of buildDeskTests().filter((x) => !(lockoutRisk && x.id === 'desk_security'))) {
  const out = await t.run(ctx);
  const good = 'ask' in out ? true : out.ok;
  check(`checklist ${t.kind}: ${t.title}`, good, 'ask' in out ? '(asks you)' : out.detail);
}

// ---- security ----
// 1. replay: a recorded request must not work twice
const raw = async (path: string, init?: RequestInit) => fetch(`http://${host}${path}`, init);
const key = fromHex(creds.key);
const chal = ((await (await raw('/dl/challenge')).json()) as { c: string }).c;
const sealed = toHex(seal(key, fromHex(chal), 'cmd', JSON.stringify({ op: 'state' })));
const req = { id: creds.id, c: chal, d: sealed };
const post = () => raw('/dl/cmd', { method: 'POST', body: JSON.stringify(req), headers: { 'Content-Type': 'application/json' } });
const first = await post();
const second = await post();
check('replay: first use ok, second rejected', first.status === 200 && second.status === 401, `${first.status}, ${second.status}`);

// 2. wrong key
const evil = generateKeyPair();
const evilKey = deriveKey(evil, evil.pub, evil.pub, evil.pub);
const chal2 = ((await (await raw('/dl/challenge')).json()) as { c: string }).c;
const bad = await raw('/dl/cmd', { method: 'POST', body: JSON.stringify({ id: creds.id, c: chal2, d: toHex(seal(evilKey, fromHex(chal2), 'cmd', '{"op":"move","dir":"up"}')) }) });
check('wrong key rejected', bad.status === 401);
check('desk did not move for the attacker', (await desk.state()).keys[0] === '-');

// 3. pairing is closed after the window / unknown client id
const unk = await raw('/dl/cmd', { method: 'POST', body: JSON.stringify({ id: 'deadbeef', c: chal2, d: '00' }) });
check('unknown client rejected', unk.status === 401);

// 4. lock-out after repeated failures (only when running against the mock: it would lock a real desk for a minute)
if (process.env.TEST_LOCKOUT !== '0') {
  for (let i = 0; i < 6; i++) await raw('/dl/cmd', { method: 'POST', body: JSON.stringify({ id: 'deadbeef', c: '00', d: '00' }) });
  const locked = await raw('/dl/cmd', { method: 'POST', body: '{}' });
  check('lock-out after 5 bad messages', locked.status === 429, String(locked.status));
  try { await desk.state(); check('even a valid client is locked out', false); } catch (e) { check('even a valid client is locked out', e instanceof DeskError && e.status === 429); }
}

console.log(failed ? `\n${failed} FAILED` : '\nall passed');
process.exit(failed ? 1 : 0);

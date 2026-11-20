// Smoke test of the API client against a running robot (real or mock).
//   node tools/mock-robot.mjs &          (in another terminal)
//   npx tsx tools/smoke.mts localhost:8787 chiku
//   npx tsx tools/smoke.mts localhost:8788 jinu
import { RobotClient, RobotError } from '../src/api/client';

const host = process.argv[2] ?? 'localhost:8787';
const kind = process.argv[3] ?? 'chiku';
const c = new RobotClient(host);
let failed = 0;
const check = (name: string, ok: boolean, extra = '') => {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${name} ${extra}`);
  if (!ok) failed++;
};
const wait = (ms: number) => new Promise((r) => setTimeout(r, ms));

const s0 = await c.state();
check('state shape', s0.api === 1 && s0.robot === kind && Array.isArray(s0.servo) && s0.servo.length === (kind === 'chiku' ? 5 : 4), `${s0.robot} fw ${s0.fw}`);

await c.setMode('manual');
check('mode manual', (await c.state()).mode === 'manual');
await c.setMode('explore');
const se = await c.state();
check('mode explore turns avoid on', se.mode === 'explore' && se.avoid === true);
await c.setMode('auto');

try {
  await fetch(`http://${host}/api/mode?m=bogus`, { method: 'POST' }).then((r) => check('bad mode rejected (400)', r.status === 400));
} catch (e) {
  check('bad mode rejected', false, String(e));
}

await c.setMute(true);
check('mute on', (await c.state()).mute === true);
await c.setMute(false);

await c.mood(7, 0);
check('mood love', (await c.state()).mood === 'love');
await c.mood(0, 0);

await c.motion('dance', 2);
const m = await c.state();
check('motion dance', m.gait === 'dance' && m.busy);
await c.stopMotion();
check('motion stop', (await c.state()).busy === false);

await c.servoSet(1, 120);
check('servo set', (await c.state()).servo[1].cur === 120);
await c.servoRelease();

const before = (await c.events(0)).next;
await c.simPad(2, 3);
await wait(50);
const ev = await c.events(before);
check('pad event (leg L triple)', ev.events.some((e) => e.type === 'pad' && e.a === 2 && e.b === 3), JSON.stringify(ev.events.at(-1)));
check('pad bit shows in state', ((await c.state()).pads & 4) !== 0);

await c.saveSettings({ brightness: 120, volume: 30 });
const st = await c.state();
check('settings saved', st.brightness === 120 && st.volume === 30);
await c.saveSettings({ servo: st.servo.map((s) => ({ trim: 3, min: s.min, max: s.max, inv: s.inv })) });
check('servo trim saved', (await c.state()).servo[0].trim === 3);

for (const call of [() => c.say('hello'), () => c.beep(440, 200), () => c.melody(1), () => c.stopAudio(), () => c.look(0.5, -0.2), () => c.display(1), () => c.display(0)]) {
  try { await call(); } catch (e) { check('simple call', false, String(e)); }
}
check('voice list', (await c.voiceList()).includes('hello'));
if (kind === 'chiku') {
  await c.screen(2);
  check('screen change', (await c.state()).screen === 2);
  await c.startTimer(60);
  check('timer', ((await c.state()).timers ?? [])[0] === 60);
  await c.cam('wake');
}

try {
  await new RobotClient('127.0.0.1:1', 800).state();
  check('unreachable host throws', false);
} catch (e) {
  check('unreachable host throws RobotError', e instanceof RobotError, (e as Error).message);
}

console.log(failed ? `\n${failed} FAILED` : '\nall passed');
process.exit(failed ? 1 : 0);

// The Console command line. Friendly commands ("mood happy") and raw requests ("POST /api/beep?hz=440&ms=300").
import { EMOTES, MELODIES, MOODS, MOVES, SCREENS } from '@/api/catalog';
import type { RobotClient } from '@/api/client';
import type { Mode } from '@/api/types';

export interface CommandResult {
  ok: boolean;
  text: string;
}

export const HELP = `Commands:
  mode auto|manual|explore      mute on|off
  mood <name|0-15> [holdMs]     look <x> <y>   (-1..1)
  move <gait> [cycles]          stop      explore on|off
  servo <ch> <deg> [speed]      release
  say <clip>   beep <hz> [ms]   melody <name|0-7>   quiet
  display <0-6>   led <r> <g> <b>|off   screen <name|0-8>
  pad <0-4> tap|double|triple|long|release
  sim <shake|pickup|putdown|fall|tap|obstacle|touch|alarm|button>
  set brightness|volume <n>     timer <seconds>     cam wake|sleep|led_on|led_off|snap
  state    diag    reboot
  desk state|up|down|stop|preset <n>|save <n>|tap <key> [ms]|height <cm>|arm on|off|log|raw|clients|pair [host]|forget
  GET /api/...   POST /api/...?x=1 [json body]   (any request)
Gaits: ${[...MOVES, ...EMOTES].join(' ')}`;

const num = (s: string | undefined, d?: number) => {
  const n = Number(s);
  if (s === undefined || Number.isNaN(n)) {
    if (d !== undefined) return d;
    throw new Error(`"${s ?? ''}" is not a number`);
  }
  return n;
};

const index = (list: readonly string[], s: string, what: string) => {
  const i = list.findIndex((x) => x.toLowerCase() === s.toLowerCase());
  if (i >= 0) return i;
  const n = Number(s);
  if (Number.isInteger(n) && n >= 0 && n < list.length) return n;
  throw new Error(`unknown ${what} "${s}" (${list.join(', ')})`);
};

const GESTURE_IDS: Record<string, number> = { tap: 1, double: 2, triple: 3, long: 4, release: 5 };

/** Handles the `desk ...` commands (needs the app stores, so it is passed in by the Console). */
export type DeskHandler = (args: string[]) => Promise<CommandResult>;

export async function runCommand(client: RobotClient | undefined, input: string, desk?: DeskHandler): Promise<CommandResult> {
  const line = input.trim();
  if (!line) return { ok: true, text: '' };
  if (line === 'help' || line === '?') return { ok: true, text: HELP };

  // raw request: GET /path   or   POST /path?x=1 {json}
  const raw = /^(GET|POST)\s+(\S+)(?:\s+(.*))?$/i.exec(line);
  if (raw) {
    if (!client) throw new Error('no robot selected');
    const r = await client.raw(raw[1].toUpperCase() as 'GET' | 'POST', raw[2], raw[3]);
    let text = r.text;
    try {
      text = JSON.stringify(JSON.parse(r.text), null, 1);
    } catch {
      /* not JSON */
    }
    return { ok: r.status >= 200 && r.status < 300, text: `HTTP ${r.status} (${r.ms} ms)\n${text.slice(0, 1500)}` };
  }

  const [cmd, ...a] = line.split(/\s+/);
  if (cmd.toLowerCase() === 'desk') {
    if (!desk) throw new Error('desk commands are not available here');
    return desk(a);
  }
  if (!client) throw new Error('no robot selected (desk commands work without one)');
  const reply = (r: { ok: boolean; msg?: string }) => ({ ok: r.ok, text: r.msg ? `ok: ${r.msg}` : 'ok' });

  switch (cmd.toLowerCase()) {
    case 'mode': {
      if (!['auto', 'manual', 'explore'].includes(a[0])) throw new Error('mode auto|manual|explore');
      return reply(await client.setMode(a[0] as Mode));
    }
    case 'mute':
      return reply(await client.setMute(a[0] !== 'off' && a[0] !== '0'));
    case 'mood':
      return reply(await client.mood(index(MOODS, a[0] ?? '', 'mood'), num(a[1], 5000)));
    case 'look':
      return reply(await client.look(num(a[0]), num(a[1], 0)));
    case 'move':
      return reply(await client.motion(a[0] ?? '', num(a[1], 4)));
    case 'stop':
      return reply(await client.stopMotion());
    case 'explore':
      return reply((await client.raw('POST', `/api/motion?avoid=${a[0] === 'off' ? 0 : 1}`)).status === 200 ? { ok: true } : { ok: false, msg: 'failed' });
    case 'servo':
      return reply(await client.servoSet(num(a[0]), num(a[1]), num(a[2], 90)));
    case 'release':
      return reply(await client.servoRelease());
    case 'say':
      return reply(await client.say(a[0] ?? 'hello'));
    case 'beep':
      return reply(await client.beep(num(a[0], 440), num(a[1], 300)));
    case 'melody':
      return reply(await client.melody(index(MELODIES, a[0] ?? '0', 'melody')));
    case 'quiet':
      return reply(await client.stopAudio());
    case 'display':
      return reply(await client.display(num(a[0])));
    case 'led':
      return reply(a[0] === 'off' ? await client.ledOff() : await client.led(num(a[0]), num(a[1]), num(a[2])));
    case 'screen':
      return reply(await client.screen(index(SCREENS, a[0] ?? '0', 'screen')));
    case 'pad': {
      const g = GESTURE_IDS[a[1] ?? 'tap'];
      if (!g) throw new Error('gesture: tap double triple long release');
      return reply(await client.simPad(num(a[0]), g));
    }
    case 'sim':
      return reply(await client.simEvent(a[0] as Parameters<RobotClient['simEvent']>[0]));
    case 'set': {
      if (a[0] !== 'brightness' && a[0] !== 'volume') throw new Error('set brightness|volume <n>');
      return reply(await client.saveSettings({ [a[0]]: num(a[1]) }));
    }
    case 'timer':
      return reply(await client.startTimer(num(a[0], 60)));
    case 'cam':
      return reply(await client.cam((a[0] ?? 'ip') as Parameters<RobotClient['cam']>[0]));
    case 'reboot':
      return reply(await client.reboot());
    case 'state': {
      const s = await client.state();
      return { ok: true, text: JSON.stringify(s, null, 1) };
    }
    case 'diag': {
      const d = await client.diag();
      return { ok: d.items.every((i) => i.ok), text: d.items.map((i) => `${i.ok ? 'ok  ' : 'FAIL'} ${i.name}: ${i.detail}`).join('\n') };
    }
    default:
      throw new Error(`unknown command "${cmd}". Type help.`);
  }
}

// The hardware checklist: one test for every screen, sensor, touch pad, motor and sound.
// Three kinds of test:
//   auto    - the app decides pass/fail itself from robot data
//   watch   - the app asks you to do something (touch a pad, wave a hand) and waits for the robot to notice
//   confirm - the app makes the robot do something and asks "did it work?" (you see / hear it)
import { DISPLAY_PATTERNS, MOODS, PAD_NAMES, SERVO_NAMES } from '@/api/catalog';
import type { RobotClient } from '@/api/client';
import type { RobotKind, RobotState } from '@/api/types';
import type { DeskLinkClient } from '@/desk/client';

export type Group = 'Overview' | 'Screen' | 'Touch pads' | 'Sensors' | 'Motors' | 'Sound' | 'Network' | 'Desk';

export interface Ctx {
  /** the selected robot (desk tests do not use it and also run without a robot) */
  client: RobotClient;
  /** the paired desk module (only set for desk tests) */
  desk?: DeskLinkClient;
  kind: RobotKind;
  /** show progress text on the test row while it runs */
  note: (text: string) => void;
  sleep: (ms: number) => Promise<void>;
  /** number of ERROR lines the robot logged since the checklist started */
  errorsSince: () => number;
}

export type Outcome = { ok: boolean; detail: string } | { ask: string; detail?: string };

export interface TestDef {
  id: string;
  group: Group;
  title: string;
  /** what to do / what the test checks */
  help: string;
  only?: RobotKind;
  kind: 'auto' | 'watch' | 'confirm';
  run: (ctx: Ctx) => Promise<Outcome>;
}

const sleep = (ms: number) => new Promise<void>((r) => setTimeout(r, ms));

/** Polls `read` until `done(value)` is true or the time is up. */
async function waitFor<T>(ctx: Ctx, seconds: number, read: () => Promise<T>, done: (v: T) => boolean, hint: (v: T | undefined, left: number) => string) {
  const end = Date.now() + seconds * 1000;
  let last: T | undefined;
  while (Date.now() < end) {
    try {
      last = await read();
      if (done(last)) return { hit: true, last };
    } catch {
      /* keep waiting */
    }
    ctx.note(hint(last, Math.ceil((end - Date.now()) / 1000)));
    await sleep(250);
  }
  return { hit: false, last };
}

const state = (c: Ctx) => c.client.state();

async function eventsAfter(c: Ctx) {
  const since = (await c.client.events(0)).next;
  return since;
}

/** Wait for an event of `type` (optionally matching a predicate) that happens after now. */
async function waitEvent(ctx: Ctx, seconds: number, match: (e: { type: string; a: number; b: number }) => boolean, hint: string) {
  const since = await eventsAfter(ctx);
  const r = await waitFor(ctx, seconds, () => ctx.client.events(since), (v) => v.events.some(match), (_v, left) => `${hint} (${left} s)`);
  return r.hit;
}

async function diagItem(ctx: Ctx, name: string): Promise<Outcome> {
  const d = await ctx.client.diag();
  const it = d.items.find((i) => i.name === name);
  if (!it) return { ok: false, detail: `no "${name}" entry in /api/diag` };
  return { ok: it.ok, detail: it.detail || (it.ok ? 'ok' : 'failed') };
}

export function buildTests(kind: RobotKind): TestDef[] {
  const pads = PAD_NAMES[kind];
  const servos = SERVO_NAMES[kind];
  const tests: TestDef[] = [];
  const add = (t: TestDef) => tests.push(t);

  // ------------------------------------------------ overview
  add({
    id: 'diag_all', group: 'Overview', title: 'Start-up self check', kind: 'auto',
    help: 'Reads the results of every hardware check the robot made at boot, plus live checks.',
    run: async (ctx) => {
      const d = await ctx.client.diag();
      const bad = d.items.filter((i) => !i.ok);
      return bad.length === 0
        ? { ok: true, detail: `${d.items.length} checks ok` }
        : { ok: false, detail: bad.map((i) => `${i.name}: ${i.detail || 'failed'}`).join('; ') };
    },
  });
  add({
    id: 'log_clean', group: 'Overview', title: 'No errors in the robot log', kind: 'auto',
    help: 'Counts ERROR lines in the robot log since you opened the app. Run it after the other tests.',
    run: async (ctx) => {
      const n = ctx.errorsSince();
      return { ok: n === 0, detail: n === 0 ? 'no error lines' : `${n} error line(s): see Console > Robot log` };
    },
  });

  // ------------------------------------------------ screen
  add({
    id: 'display_colors', group: 'Screen', title: 'Display colours', kind: 'confirm',
    help: 'Shows red, green, blue, white, black and a grey ramp. Look for dead pixels, wrong colours, banding.',
    run: async (ctx) => {
      for (const p of DISPLAY_PATTERNS.filter((x) => x.id > 0)) {
        ctx.note(`showing ${p.label}`);
        await ctx.client.display(p.id);
        await ctx.sleep(1100);
      }
      await ctx.client.display(0);
      return { ask: 'Did you see all six patterns correctly (no dead pixels, no wrong colour, smooth ramp)?' };
    },
  });
  add({
    id: 'display_faces', group: 'Screen', title: 'All faces', kind: 'confirm',
    help: 'Shows every face for 1.5 s. Check that each one looks right and the animation is smooth.',
    run: async (ctx) => {
      await ctx.client.setMode('manual');
      for (let i = 0; i < MOODS.length; i++) {
        ctx.note(`face ${i + 1}/${MOODS.length}: ${MOODS[i]}`);
        await ctx.client.mood(i, 0);
        await ctx.sleep(1500);
      }
      await ctx.client.mood(0, 0);
      return { ask: 'Were all 16 faces drawn correctly and smoothly (no flicker or tearing)?' };
    },
  });
  add({
    id: 'brightness', group: 'Screen', title: 'Brightness', kind: 'confirm',
    help: 'Sweeps the brightness from low to high and back.',
    run: async (ctx) => {
      const before = (await state(ctx)).brightness;
      for (const b of [20, 80, 160, 255, 80, before]) {
        ctx.note(`brightness ${Math.round((b * 100) / 255)}%`);
        await ctx.client.saveSettings({ brightness: b });
        await ctx.sleep(900);
      }
      return { ask: 'Did the screen get darker and brighter smoothly?' };
    },
  });
  if (kind === 'chiku') {
    add({
      id: 'touch_tap', group: 'Screen', title: 'Touch screen: tap', kind: 'watch', only: 'chiku',
      help: 'Tap anywhere on the robot screen.',
      run: async (ctx) => {
        const ok = await waitEvent(ctx, 20, (e) => e.type === 'tap', 'tap the robot screen');
        return { ok, detail: ok ? 'tap received' : 'no tap seen in 20 s' };
      },
    });
    add({
      id: 'touch_swipe', group: 'Screen', title: 'Touch screen: swipe', kind: 'watch', only: 'chiku',
      help: 'Swipe left or right on the robot screen.',
      run: async (ctx) => {
        const ok = await waitEvent(ctx, 20, (e) => e.type === 'swipe', 'swipe the robot screen');
        return { ok, detail: ok ? 'swipe received' : 'no swipe seen in 20 s' };
      },
    });
    add({
      id: 'screens', group: 'Screen', title: 'Screens', kind: 'confirm', only: 'chiku',
      help: 'Switches through Face, Clock, Weather, Timer, Sonar, Camera and Settings.',
      run: async (ctx) => {
        for (let i = 0; i <= 6; i++) {
          ctx.note(`screen ${i}`);
          await ctx.client.screen(i);
          await ctx.sleep(1500);
        }
        await ctx.client.screen(0);
        return { ask: 'Did every screen appear with sensible content?' };
      },
    });
  } else {
    add({
      id: 'rgb_led', group: 'Screen', title: 'RGB LED', kind: 'confirm', only: 'jinu',
      help: 'Lights the onboard LED red, green, blue and white.',
      run: async (ctx) => {
        for (const [n, r, g, b] of [['red', 255, 0, 0], ['green', 0, 255, 0], ['blue', 0, 0, 255], ['white', 255, 255, 255]] as const) {
          ctx.note(n);
          await ctx.client.led(r, g, b);
          await ctx.sleep(1000);
        }
        await ctx.client.ledOff();
        return { ask: 'Did the LED show red, green, blue and white?' };
      },
    });
  }

  // ------------------------------------------------ touch pads
  add({
    id: 'pad_expander', group: 'Touch pads', title: 'Pad expander (PCF8574)', kind: 'auto',
    help: 'Checks that the I2C expander for the pads answers.',
    run: (ctx) => diagItem(ctx, 'pad_expander'),
  });
  pads.forEach((name, i) =>
    add({
      id: `pad_${i}`, group: 'Touch pads', title: `Pad: ${name}`, kind: 'watch',
      help: `Touch the ${name} pad once.`,
      run: async (ctx) => {
        const ok = await waitEvent(ctx, 20, (e) => e.type === 'pad' && e.a === i, `touch the ${name} pad`);
        return { ok, detail: ok ? 'touch received' : 'no touch seen in 20 s (check wiring / TTP223)' };
      },
    }),
  );
  add({
    id: 'pad_gestures', group: 'Touch pads', title: 'Body pad gestures', kind: 'watch',
    help: 'On the body pad: double tap, then triple tap, then a long press (hold about 1 s).',
    run: async (ctx) => {
      const need = new Set([2, 3, 4]);
      const since = await eventsAfter(ctx);
      const r = await waitFor(
        ctx, 40, () => ctx.client.events(since),
        (v) => { v.events.filter((e) => e.type === 'pad' && e.a === 4).forEach((e) => need.delete(e.b)); return need.size === 0; },
        (_v, left) => `still needed: ${[...need].map((g) => ({ 2: 'double', 3: 'triple', 4: 'long' })[g]).join(', ')} (${left} s)`,
      );
      return { ok: r.hit, detail: r.hit ? 'double, triple and long press seen' : 'missing: ' + [...need].map((g) => ({ 2: 'double', 3: 'triple', 4: 'long' })[g]).join(', ') };
    },
  });

  // ------------------------------------------------ sensors
  add({
    id: 'sonar_reading', group: 'Sensors', title: 'Sonar: reading', kind: 'auto',
    help: 'The sonar must report a distance between 2 and 400 cm.',
    run: async (ctx) => {
      const s = await state(ctx);
      return { ok: s.dist >= 2 && s.dist <= 400, detail: s.dist < 0 ? 'no echo' : `${s.dist} cm` };
    },
  });
  add({
    id: 'sonar_range', group: 'Sensors', title: 'Sonar: near and far', kind: 'watch',
    help: 'Hold your hand about 10 cm in front of the sonar, then take it away (more than 40 cm).',
    run: async (ctx) => {
      let near = false;
      const r = await waitFor(
        ctx, 30, () => state(ctx),
        (s) => { if (s.dist > 0 && s.dist < 15) near = true; return near && s.dist > 40; },
        (s, left) => `${near ? 'now take the hand away' : 'hold the hand 10 cm away'} - reads ${s?.dist ?? '?'} cm (${left} s)`,
      );
      return { ok: r.hit, detail: r.hit ? 'saw < 15 cm and > 40 cm' : near ? 'near seen but never far' : 'never saw < 15 cm' };
    },
  });
  if (kind === 'chiku') {
    add({ id: 'imu_gravity', group: 'Sensors', title: 'IMU: gravity', kind: 'auto', only: 'chiku', help: 'Total acceleration must be about 1 g when the robot is still.', run: (ctx) => diagItem(ctx, 'imu_gravity') });
    add({
      id: 'imu_tilt', group: 'Sensors', title: 'IMU: tilt', kind: 'watch', only: 'chiku',
      help: 'Tilt the robot by more than 30 degrees, then put it upright again.',
      run: async (ctx) => {
        let tilted = false;
        const r = await waitFor(
          ctx, 30, () => state(ctx),
          (s) => { if ((s.imu?.tilt ?? 0) > 30) tilted = true; return tilted && (s.imu?.tilt ?? 99) < 10; },
          (s, left) => `${tilted ? 'now stand it up again' : 'tilt it'} - tilt ${Math.round(s?.imu?.tilt ?? 0)}° (${left} s)`,
        );
        return { ok: r.hit, detail: r.hit ? 'tilt > 30° and back' : 'not seen' };
      },
    });
    add({
      id: 'imu_shake', group: 'Sensors', title: 'IMU: shake', kind: 'watch', only: 'chiku',
      help: 'Shake the robot for a second.',
      run: async (ctx) => {
        const ok = await waitEvent(ctx, 20, (e) => e.type === 'shake', 'shake the robot');
        return { ok, detail: ok ? 'shake detected' : 'no shake detected in 20 s' };
      },
    });
    add({
      id: 'clock', group: 'Sensors', title: 'Clock (RTC / NTP)', kind: 'auto', only: 'chiku',
      help: 'The clock chip must answer and the time must be valid (needs Wi-Fi once).',
      run: async (ctx) => {
        const d = await ctx.client.diag();
        const rtc = d.items.find((i) => i.name === 'rtc');
        const t = d.items.find((i) => i.name === 'rtc_time');
        return { ok: !!rtc?.ok && !!t?.ok, detail: `chip ${rtc?.ok ? 'ok' : 'missing'}, time ${t?.ok ? 'valid' : 'not set'}` };
      },
    });
    add({
      id: 'battery', group: 'Sensors', title: 'Battery / PMU', kind: 'auto', only: 'chiku',
      help: 'The power chip must answer and report a battery.',
      run: async (ctx) => {
        const s = await state(ctx);
        return { ok: (s.batt ?? -1) >= 0, detail: (s.batt ?? -1) < 0 ? 'no battery reported' : `${s.batt}%${s.charging ? ' charging' : ''}` };
      },
    });
    add({ id: 'cam_link', group: 'Sensors', title: 'ESP32-CAM link', kind: 'auto', only: 'chiku', help: 'The cam must answer over the UART link.', run: (ctx) => diagItem(ctx, 'cam_link') });
    add({
      id: 'cam_snap', group: 'Sensors', title: 'Camera snapshot', kind: 'auto', only: 'chiku',
      help: 'Wakes the camera and takes a picture; the cam reports its size.',
      run: async (ctx) => {
        const before = (await state(ctx)).cam?.snap ?? 0;
        await ctx.client.cam('wake');
        await ctx.sleep(1500);
        await ctx.client.cam('snap');
        const r = await waitFor(ctx, 10, () => state(ctx), (s) => (s.cam?.snap ?? 0) > 1000 && (s.cam?.snap ?? 0) !== before, (_s, left) => `waiting for the picture (${left} s)`);
        const bytes = r.last?.cam?.snap ?? 0;
        return { ok: r.hit, detail: r.hit ? `${bytes} bytes` : 'no picture reported (cam power? wiring?)' };
      },
    });
    add({
      id: 'cam_light', group: 'Sensors', title: 'Camera light', kind: 'confirm', only: 'chiku',
      help: 'Switches the white LED on the ESP32-CAM on for 2 seconds.',
      run: async (ctx) => {
        await ctx.client.cam('led_on');
        await ctx.sleep(2000);
        await ctx.client.cam('led_off');
        return { ask: 'Did the cam LED light up?' };
      },
    });
  }

  // ------------------------------------------------ motors
  if (kind === 'chiku') add({ id: 'servo_driver', group: 'Motors', title: 'Servo driver (PCA9685)', kind: 'auto', only: 'chiku', help: 'The PCA9685 must answer on I2C.', run: (ctx) => diagItem(ctx, 'servo_driver') });
  servos.forEach((name, i) =>
    add({
      id: `servo_${i}`, group: 'Motors', title: `Servo ${i}: ${name}`, kind: 'confirm',
      help: 'Moves the servo to 90, 110, 70 and back to 90 degrees slowly. Keep legs free to move.',
      run: async (ctx) => {
        await ctx.client.setMode('manual');
        for (const deg of [90, 110, 70, 90]) {
          ctx.note(`moving to ${deg}°`);
          await ctx.client.servoSet(i, deg, 60);
          await ctx.sleep(1100);
        }
        const cur = (await state(ctx)).servo[i]?.cur;
        return { ask: `Did "${name}" swing left and right smoothly without buzzing? (commanded ${cur}°)` };
      },
    }),
  );
  add({
    id: 'servo_together', group: 'Motors', title: 'All servos together (power test)', kind: 'auto',
    help: 'Moves all servos at once. The robot must not reset (brown-out) while they start.',
    run: async (ctx) => {
      const up0 = (await state(ctx)).uptime;
      for (const deg of [120, 60, 90]) {
        ctx.note(`all servos to ${deg}°`);
        for (let i = 0; i < servos.length; i++) await ctx.client.servoSet(i, deg, 200);
        await ctx.sleep(1500);
      }
      try {
        const s = await state(ctx);
        return s.uptime >= up0 ? { ok: true, detail: 'no reset during the move' } : { ok: false, detail: 'the robot rebooted: supply too weak?' };
      } catch {
        return { ok: false, detail: 'robot stopped answering: brown-out? check the servo supply' };
      }
    },
  });
  add({
    id: 'servo_release', group: 'Motors', title: 'Servos release (no holding current)', kind: 'confirm',
    help: 'Turns the servo outputs off. The legs should become loose.',
    run: async (ctx) => {
      await ctx.client.servoRelease();
      return { ask: 'Can you now move the legs freely by hand (servos limp)?' };
    },
  });
  add({
    id: 'gait_wiggle', group: 'Motors', title: 'Movement: wiggle and walk', kind: 'confirm',
    help: 'Wiggles, then walks 3 steps. Put the robot on the floor with free space around it.',
    run: async (ctx) => {
      await ctx.client.setMode('manual');
      ctx.note('wiggle');
      await ctx.client.motion('wiggle', 4);
      await ctx.sleep(2200);
      ctx.note('walk');
      await ctx.client.motion('walk', 3);
      await ctx.sleep(3500);
      await ctx.client.stopMotion();
      return { ask: 'Did it wiggle and then walk forward (not backward, not falling over)?' };
    },
  });

  // ------------------------------------------------ sound
  add({ id: 'audio_init', group: 'Sound', title: 'Audio chip', kind: 'auto', help: 'The audio output must have started.', run: (ctx) => diagItem(ctx, 'audio') });
  add({
    id: 'sound_tones', group: 'Sound', title: 'Tones', kind: 'confirm',
    help: 'Plays 262, 440, 880 and 1760 Hz.',
    run: async (ctx) => {
      await ctx.client.setMute(false);
      for (const hz of [262, 440, 880, 1760]) {
        ctx.note(`${hz} Hz`);
        await ctx.client.beep(hz, 500);
        await ctx.sleep(750);
      }
      return { ask: 'Did you hear four clean tones, rising in pitch (no crackle)?' };
    },
  });
  add({
    id: 'sound_melody', group: 'Sound', title: 'Melody', kind: 'confirm',
    help: 'Plays the happy melody.',
    run: async (ctx) => {
      await ctx.client.melody(2);
      await ctx.sleep(1200);
      return { ask: 'Did you hear a short happy tune?' };
    },
  });
  add({
    id: 'sound_voice', group: 'Sound', title: 'Voice', kind: 'confirm',
    help: 'Says "hello" (recorded clip, or babble if the clip is missing).',
    run: async (ctx) => {
      await ctx.client.say('hello');
      await ctx.sleep(2500);
      return { ask: 'Did you hear Hello (or a cute babble)?' };
    },
  });
  add({
    id: 'sound_volume', group: 'Sound', title: 'Volume', kind: 'confirm',
    help: 'Plays a tone at 20, 50 and 90 percent volume and restores your volume.',
    run: async (ctx) => {
      const before = (await state(ctx)).volume;
      for (const v of [20, 50, 90, before]) {
        ctx.note(`volume ${v}%`);
        await ctx.client.saveSettings({ volume: v });
        await ctx.client.beep(660, 400);
        await ctx.sleep(800);
      }
      return { ask: 'Did the tone get louder step by step (no distortion at 90%)?' };
    },
  });

  // ------------------------------------------------ network
  add({
    id: 'wifi_signal', group: 'Network', title: 'Wi-Fi signal', kind: 'auto',
    help: 'Signal strength must be better than -75 dBm.',
    run: async (ctx) => {
      const s = await state(ctx);
      return { ok: s.rssi > -75, detail: `${s.rssi} dBm` };
    },
  });
  add({
    id: 'memory', group: 'Network', title: 'Memory', kind: 'auto',
    help: 'Free heap must be above 40 KB (and PSRAM above 1 MB on Chiku).',
    run: async (ctx) => {
      const s: RobotState = await state(ctx);
      const psramOk = s.psram === undefined || s.psram > 1_000_000;
      return { ok: s.heap > 40_000 && psramOk, detail: `heap ${Math.round(s.heap / 1024)} KB${s.psram !== undefined ? `, PSRAM ${Math.round(s.psram / 1024)} KB` : ''}` };
    },
  });

  return tests;
}

// ------------------------------------------------ standing desk (module inside the handset)
export function buildDeskTests(): TestDef[] {
  const tests: TestDef[] = [];
  const add = (t: Omit<TestDef, 'group'>) => tests.push({ ...t, group: 'Desk' });
  const d = (c: Ctx) => {
    if (!c.desk) throw new Error('no desk paired');
    return c.desk;
  };
  add({
    id: 'desk_reach', title: 'Desk module reachable', kind: 'auto',
    help: 'The desk module answers on Wi-Fi.',
    run: async (ctx) => {
      const i = await d(ctx).info();
      return { ok: i.dev === 'desk', detail: `firmware ${i.fw}, ${i.clients} paired client(s)${i.pairing ? `, pairing open ${i.left} s` : ''}` };
    },
  });
  add({
    id: 'desk_auth', title: 'Secure link works', kind: 'auto',
    help: 'Sends a sealed command (challenge + AES-GCM) and verifies the sealed answer.',
    run: async (ctx) => {
      const t0 = Date.now();
      const s = await d(ctx).state(false);
      return { ok: s.ok, detail: `answer verified in ${Date.now() - t0} ms` };
    },
  });
  add({
    id: 'desk_security', title: 'Replay and wrong key are rejected', kind: 'auto',
    help: 'A recorded request must not work twice; a message sealed with another key must be refused. Uses 2 of the 5 bad messages allowed per minute.',
    run: async (ctx) => {
      const r = await d(ctx).securityProbe();
      return { ok: r.replayRejected && r.wrongKeyRejected, detail: `replay ${r.replayRejected ? 'rejected' : 'ACCEPTED'}, wrong key ${r.wrongKeyRejected ? 'rejected' : 'ACCEPTED'}` };
    },
  });
  add({
    id: 'desk_height', title: 'Height feedback', kind: 'auto',
    help: 'The module must know the height (read from the display line, or estimated and calibrated).',
    run: async (ctx) => {
      const s = await d(ctx).state(false);
      if (!s.hv) return { ok: false, detail: 'unknown: enter the display height in Control > Desk (calibrate)' };
      return { ok: true, detail: `${s.h.toFixed(1)} cm from the ${s.src === 'display' ? 'display line' : 'estimate (display line not read)'}` };
    },
  });
  add({
    id: 'desk_display', title: 'Display line decoded (optional)', kind: 'auto',
    help: 'Only when the handset data line is wired to the module: frames must arrive with a good checksum.',
    run: async (ctx) => {
      const s = await d(ctx).state(false);
      return { ok: s.frames > 0, detail: s.frames > 0 ? `${s.frames} frames ok, ${s.bad} bad, display "${s.disp}"` : s.bytes > 0 ? `${s.bytes} bytes but no valid frame: check the baud rate and use Raw capture` : 'no data on the line (not wired = estimate is used)' };
    },
  });
  add({
    id: 'desk_childlock', title: 'Child lock', kind: 'auto',
    help: 'With the child lock on, a move command must be refused. The lock is switched off again afterwards.',
    run: async (ctx) => {
      const c = d(ctx);
      await c.arm(false);
      const r = await c.move('up', 100);
      await c.arm(true);
      return { ok: r.ok === false, detail: r.ok ? 'the move was NOT refused' : `refused: ${r.err}` };
    },
  });
  add({
    id: 'desk_up', title: 'Nudge UP (dead-man)', kind: 'confirm',
    help: 'Sends ONE short move message. The desk rises about 1 cm and must then stop by itself. Keep the desk area clear.',
    run: async (ctx) => {
      const c = d(ctx);
      const before = await c.state(false);
      await c.move('up', 450);
      await ctx.sleep(1400);
      const after = await c.state(false);
      if (after.keys[0] === '#') {
        await c.stop();
        return { ok: false, detail: 'UP stayed pressed after the keep-alive ended: safety failure' };
      }
      return { ask: `Did the desk rise a little and stop by itself? (${before.h.toFixed(1)} to ${after.h.toFixed(1)} cm)` };
    },
  });
  add({
    id: 'desk_down', title: 'Nudge DOWN (dead-man)', kind: 'confirm',
    help: 'Sends ONE short move message. The desk lowers about 1 cm and must then stop by itself.',
    run: async (ctx) => {
      const c = d(ctx);
      const before = await c.state(false);
      await c.move('down', 450);
      await ctx.sleep(1400);
      const after = await c.state(false);
      if (after.keys[1] === '#') {
        await c.stop();
        return { ok: false, detail: 'DOWN stayed pressed after the keep-alive ended: safety failure' };
      }
      return { ask: `Did the desk lower a little and stop by itself? (${before.h.toFixed(1)} to ${after.h.toFixed(1)} cm)` };
    },
  });
  add({
    id: 'desk_stop', title: 'STOP', kind: 'confirm',
    help: 'Moves up while sending keep-alives, then sends STOP. The desk must stop at once.',
    run: async (ctx) => {
      const c = d(ctx);
      for (let i = 0; i < 4; i++) {
        await c.move('up', 700);
        await ctx.sleep(250);
      }
      await c.stop();
      await ctx.sleep(300);
      const s = await c.state(false);
      if (s.keys.includes('#')) return { ok: false, detail: `a key is still pressed after STOP (${s.keys})` };
      return { ask: 'Did the desk stop immediately when STOP was sent?' };
    },
  });
  for (let n = 1; n <= 4; n++) {
    add({
      id: `desk_preset_${n}`, title: `Memory position ${n}`, kind: 'confirm',
      help: `The desk drives to the height stored in memory ${n}. It only works if you saved a height there with the handset. Stand clear; STOP is in Control, Desk.`,
      run: async (ctx) => {
        const r = await d(ctx).preset(n);
        if (!r.ok) return { ok: false, detail: `refused: ${r.err}` };
        await ctx.sleep(2500);
        return { ask: `Did the desk start driving to memory ${n}? (press STOP in Control, Desk when it is enough)` };
      },
    });
  }
  add({
    id: 'desk_key_s', title: 'Key S', kind: 'confirm',
    help: 'Taps the S key once (150 ms).',
    run: async (ctx) => {
      await d(ctx).tap('s', 150);
      await ctx.sleep(600);
      return { ask: 'Did the handset react to S (display or beep)?' };
    },
  });
  add({
    id: 'desk_key_x', title: 'Key X (the blank key)', kind: 'confirm',
    help: 'Taps the blank 8th key once. Its function is unknown: watch what the handset does.',
    run: async (ctx) => {
      await d(ctx).tap('x', 150);
      await ctx.sleep(600);
      return { ask: 'Did something happen (display, beep, light)? Answer No if nothing visible happened.' };
    },
  });
  return tests;
}

export const wait = sleep;

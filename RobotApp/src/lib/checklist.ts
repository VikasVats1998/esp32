// The hardware checklist: one test for every screen, sensor, touch pad, motor and sound.
// Three kinds of test:
//   auto    - the app decides pass/fail itself from robot data
//   watch   - the app asks you to do something (touch a pad, wave a hand) and waits for the robot to notice
//   confirm - the app makes the robot do something and asks "did it work?" (you see / hear it)
import { DISPLAY_PATTERNS, MOODS, PAD_NAMES, SERVO_NAMES } from '@/api/catalog';
import type { RobotClient } from '@/api/client';
import type { RobotKind, RobotState } from '@/api/types';

export type Group = 'Overview' | 'Screen' | 'Touch pads' | 'Sensors' | 'Motors' | 'Sound' | 'Network';

export interface Ctx {
  client: RobotClient;
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

export const wait = sleep;

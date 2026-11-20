// Small typed client for the robot HTTP API. No React / React Native imports here,
// so it can also be tested in plain Node (tools/smoke.ts).
import type { EventsReply, Mode, RobotState } from './types';

export class RobotError extends Error {
  constructor(message: string, readonly status?: number) {
    super(message);
    this.name = 'RobotError';
  }
}

type Params = Record<string, string | number | boolean | undefined>;

export class RobotClient {
  constructor(
    readonly host: string,
    private readonly timeoutMs = 2500,
  ) {}

  get base() {
    return `http://${this.host}`;
  }

  private url(path: string, params?: Params) {
    const q = Object.entries(params ?? {})
      .filter(([, v]) => v !== undefined)
      .map(([k, v]) => `${encodeURIComponent(k)}=${encodeURIComponent(String(typeof v === 'boolean' ? +v : v))}`)
      .join('&');
    return `${this.base}${path}${q ? `?${q}` : ''}`;
  }

  private async request(method: 'GET' | 'POST', path: string, params?: Params, body?: BodyInit | string, timeout = this.timeoutMs) {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), timeout);
    try {
      const res = await fetch(this.url(path, params), {
        method,
        body: body as BodyInit | undefined,
        headers: typeof body === 'string' ? { 'Content-Type': 'application/json' } : undefined,
        signal: ctrl.signal,
      });
      if (!res.ok) throw new RobotError(`${method} ${path}: ${res.status} ${await res.text().catch(() => '')}`, res.status);
      return res;
    } catch (e) {
      if (e instanceof RobotError) throw e;
      throw new RobotError(ctrl.signal.aborted ? `${this.host}: no answer (timeout)` : `${this.host}: ${(e as Error).message}`);
    } finally {
      clearTimeout(timer);
    }
  }

  private async json<T>(path: string, params?: Params): Promise<T> {
    return (await this.request('GET', path, params)).json() as Promise<T>;
  }

  private async post(path: string, params?: Params, body?: unknown) {
    await this.request('POST', path, params, body === undefined ? undefined : JSON.stringify(body));
  }

  // ---- read ----
  state() {
    return this.json<RobotState>('/api/state');
  }
  events(since: number) {
    return this.json<EventsReply>('/api/events', { since });
  }
  voiceList() {
    return this.json<string[]>('/api/voice/list');
  }

  // ---- modes and faces ----
  setMode(mode: Mode) {
    return this.post('/api/mode', { m: mode });
  }
  setMute(on: boolean) {
    return this.post('/api/mute', { on });
  }
  /** holdMs: 0 = stay in this mood until changed */
  mood(index: number, holdMs = 5000) {
    return this.post('/api/mood', { m: index, hold: holdMs });
  }
  look(x: number, y: number) {
    return this.post('/api/look', { x, y });
  }
  screen(id: number) {
    return this.post('/api/screen', { id });
  }
  display(pattern: number) {
    return this.post('/api/display', { p: pattern });
  }

  // ---- movement ----
  motion(gait: string, cycles = 4) {
    return this.post('/api/motion', { g: gait, n: cycles });
  }
  stopMotion() {
    return this.post('/api/motion', { stop: 1 });
  }
  servoSet(ch: number, deg: number, speed = 90) {
    return this.post('/api/servo/set', { ch, deg: Math.round(deg), speed });
  }
  servoRelease() {
    return this.post('/api/servo/release');
  }

  // ---- sound ----
  say(key: string) {
    return this.post('/api/say', { key });
  }
  beep(hz: number, ms: number) {
    return this.post('/api/beep', { hz, ms });
  }
  melody(id: number, loop = false) {
    return this.post('/api/melody', { id, loop });
  }
  stopAudio() {
    return this.post('/api/audio/stop');
  }

  // ---- simulate sensors ----
  simPad(pad: number, gesture: number) {
    return this.post('/api/sim', { pad, g: gesture });
  }
  simEvent(ev: 'shake' | 'pickup' | 'putdown' | 'fall' | 'tap' | 'obstacle' | 'touch' | 'alarm' | 'button') {
    return this.post('/api/sim', { ev });
  }

  // ---- settings ----
  saveSettings(patch: {
    brightness?: number;
    volume?: number;
    lat?: number;
    lon?: number;
    servo?: { trim: number; min: number; max: number; inv: boolean }[];
    alarms?: { on: boolean; h: number; m: number; days: number }[];
  }) {
    return this.post('/api/set', undefined, patch);
  }
  reboot() {
    return this.post('/api/reboot');
  }
  wifiReset() {
    return this.post('/api/wifi/reset');
  }

  // ---- Chiku extras ----
  startTimer(seconds: number) {
    return this.post('/api/timer', { sec: seconds });
  }
  cancelTimer(slot: number) {
    return this.post('/api/timer/cancel', { slot });
  }
  cam(cmd: 'wake' | 'sleep' | 'led_on' | 'led_off' | 'snap' | 'ip') {
    return this.post('/api/cam', { cmd });
  }
  refreshWeather() {
    return this.post('/api/weather/refresh');
  }

  // ---- files ----
  /** Upload a firmware .bin to /update (OTA). `file` is a RN file object {uri,name,type} or a Blob. */
  async uploadFirmware(file: unknown) {
    const form = new FormData();
    form.append('fw', file as Blob);
    await this.request('POST', '/update', undefined, form, 120000);
  }
  /** `file` is a RN file object {uri,name,type}; in Node pass a Blob plus `name`. */
  async uploadVoice(file: unknown, name?: string) {
    const form = new FormData();
    if (name) form.append('f', file as Blob, name);
    else form.append('f', file as Blob);
    await this.request('POST', '/api/voice', undefined, form, 30000);
  }
}

export function normaliseHost(input: string) {
  return input.trim().replace(/^https?:\/\//, '').replace(/\/.*$/, '');
}

// Small typed client for the robot HTTP API. No React / React Native imports here,
// so it can also be tested in plain Node (tools/smoke.mts).
// Every request (except the polling ones) is reported to RobotClient.onTraffic, which the
// app uses for the Traffic log and for the feedback line after each command.
import type { CommandReply, DiagReply, EventsReply, LogReply, Mode, RobotState, Traffic } from './types';

export class RobotError extends Error {
  constructor(message: string, readonly status?: number) {
    super(message);
    this.name = 'RobotError';
  }
}

type Params = Record<string, string | number | boolean | undefined>;
interface Reply {
  status: number;
  text: string;
  ms: number;
}

let trafficId = 0;

export class RobotClient {
  /** Set once by the app: called for every non-polling request. */
  static onTraffic?: (t: Traffic) => void;

  constructor(
    readonly host: string,
    private readonly timeoutMs = 2500,
  ) {}

  get base() {
    return `http://${this.host}`;
  }

  private path(path: string, params?: Params) {
    const q = Object.entries(params ?? {})
      .filter(([, v]) => v !== undefined)
      .map(([k, v]) => `${encodeURIComponent(k)}=${encodeURIComponent(String(typeof v === 'boolean' ? +v : v))}`)
      .join('&');
    return q ? `${path}?${q}` : path;
  }

  /**
   * Sends one request. Throws RobotError on network errors and (unless `lenient`) on HTTP errors.
   * `quiet` requests (polling) are not reported to onTraffic.
   */
  private async request(
    method: 'GET' | 'POST',
    pathWithQuery: string,
    opts: { body?: BodyInit | string; timeout?: number; quiet?: boolean; lenient?: boolean; label?: string } = {},
  ): Promise<Reply> {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), opts.timeout ?? this.timeoutMs);
    const started = Date.now();
    const report = (t: Partial<Traffic>) => {
      if (opts.quiet || !RobotClient.onTraffic) return;
      RobotClient.onTraffic({
        id: ++trafficId,
        at: started,
        host: this.host,
        method,
        path: opts.label ?? pathWithQuery,
        body: typeof opts.body === 'string' ? opts.body : undefined,
        ok: false,
        ms: Date.now() - started,
        ...t,
      });
    };
    try {
      const res = await fetch(`${this.base}${pathWithQuery}`, {
        method,
        body: opts.body as BodyInit | undefined,
        headers: typeof opts.body === 'string' ? { 'Content-Type': 'application/json' } : undefined,
        signal: ctrl.signal,
      });
      const text = await res.text().catch(() => '');
      const ms = Date.now() - started;
      report({ status: res.status, ok: res.ok, response: text.slice(0, 300), ms });
      if (!res.ok && !opts.lenient) throw new RobotError(`${method} ${pathWithQuery}: ${res.status} ${text}`, res.status);
      return { status: res.status, text, ms };
    } catch (e) {
      if (e instanceof RobotError) throw e;
      const message = ctrl.signal.aborted ? `${this.host}: no answer (timeout)` : `${this.host}: ${(e as Error).message}`;
      report({ error: message });
      throw new RobotError(message);
    } finally {
      clearTimeout(timer);
    }
  }

  private async getJson<T>(path: string, params?: Params, quiet = false): Promise<T> {
    const r = await this.request('GET', this.path(path, params), { quiet });
    try {
      return JSON.parse(r.text) as T;
    } catch {
      throw new RobotError(`${path}: reply is not JSON`);
    }
  }

  private async post(path: string, params?: Params, body?: unknown): Promise<CommandReply> {
    const r = await this.request('POST', this.path(path, params), { body: body === undefined ? undefined : JSON.stringify(body) });
    try {
      return JSON.parse(r.text) as CommandReply;
    } catch {
      return { ok: true };
    }
  }

  // ---- polling reads (quiet: not shown in Traffic) ----
  state() {
    return this.getJson<RobotState>('/api/state', undefined, true);
  }
  events(since: number) {
    return this.getJson<EventsReply>('/api/events', { since }, true);
  }
  log(since: number) {
    return this.getJson<LogReply>('/api/log', { since }, true);
  }

  // ---- reads shown in Traffic ----
  diag() {
    return this.getJson<DiagReply>('/api/diag');
  }
  voiceList() {
    return this.getJson<string[]>('/api/voice/list');
  }

  /** Any request by hand (console). Never throws on HTTP errors: returns status and text. */
  raw(method: 'GET' | 'POST', pathWithQuery: string, body?: string) {
    return this.request(method, pathWithQuery.startsWith('/') ? pathWithQuery : `/${pathWithQuery}`, { body, lenient: true });
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
  /** Jinu RGB LED test colour; off = follow the mood again. */
  led(r: number, g: number, b: number) {
    return this.post('/api/led', { r, g, b });
  }
  ledOff() {
    return this.post('/api/led', { off: 1 });
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

  // ---- standing desk through the robot (the robot holds its own key) ----
  /** `token` protects the robot's desk endpoints (the robot holds the desk key). The phone creates it once. */
  deskPair(host: string, token: string) {
    return this.post('/api/desk/pair', { host, token });
  }
  deskUnpair(token: string) {
    return this.post('/api/desk/unpair', { token });
  }
  deskCmd(token: string, op: 'move' | 'stop' | 'preset' | 'save' | 'tap' | 'setheight' | 'arm', params: Params = {}) {
    return this.post('/api/desk/cmd', { op, token, ...params });
  }

  // ---- files ----
  /** Upload a firmware .bin to /update (OTA). `file` is a RN file object {uri,name,type} or a Blob. */
  async uploadFirmware(file: unknown) {
    const form = new FormData();
    form.append('fw', file as Blob);
    await this.request('POST', '/update', { body: form, timeout: 120000, label: '/update (firmware upload)' });
  }
  /** `file` is a RN file object {uri,name,type}; in Node pass a Blob plus `name`. */
  async uploadVoice(file: unknown, name?: string) {
    const form = new FormData();
    if (name) form.append('f', file as Blob, name);
    else form.append('f', file as Blob);
    await this.request('POST', '/api/voice', { body: form, timeout: 30000, label: '/api/voice (clip upload)' });
  }
}

export function normaliseHost(input: string) {
  return input.trim().replace(/^https?:\/\//, '').replace(/\/.*$/, '');
}

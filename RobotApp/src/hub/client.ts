// Typed client for the hub server (Server/server.py). No React imports, so it can run in plain Node.

export type DeviceType = 'light' | 'fan' | 'switch' | 'climate' | 'other';

export interface HubDevice {
  id: string;
  name: string;
  type: DeviceType;
  on: boolean;
  /** light/fan: 0..100 %, climate: set point in degC */
  level: number;
  /** light: colour temperature in K */
  temp?: number;
  /** watts */
  load?: number;
}

export interface RobotTelemetry {
  fw?: string;
  age: number;
  tilt?: number;
  roll?: number;
  pitch?: number;
  gyro?: number;
  flip?: boolean;
  /** battery percent, -1 = no battery */
  bat?: number;
  batv?: number;
  chg?: boolean;
  usb?: boolean;
  /** core temperature, degC */
  temp?: number;
  rssi?: number;
  /** free internal heap, KB */
  heap?: number;
  /** microphone loudness 0..100 */
  snd?: number;
  clapWake?: boolean;
  clapSens?: number;
  wakeWord?: boolean;
}

export interface HubState {
  devices: HubDevice[];
  env: { temp_c?: number; humidity?: number };
  ai: { enabled: boolean };
  robots: Record<string, RobotTelemetry>;
}

export interface HistorySample {
  t: number;
  snd?: number;
  tilt?: number;
  roll?: number;
  pitch?: number;
  gyro?: number;
  temp?: number;
  bat?: number;
  rssi?: number;
  heap?: number;
}

export type RobotCfg = Partial<{ clapWake: boolean; clapSens: number; wakeWord: boolean; calibrate: boolean }>;

export class HubClient {
  constructor(
    readonly base: string,
    private readonly token = 'chiku',
    private readonly timeoutMs = 3000,
  ) {}

  private async call<T>(method: 'GET' | 'POST', path: string, body?: unknown): Promise<T> {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), this.timeoutMs);
    try {
      const res = await fetch(`${this.base}${path}`, {
        method,
        headers: { 'Content-Type': 'application/json', 'X-Token': this.token },
        body: body === undefined ? undefined : JSON.stringify(body),
        signal: ctrl.signal,
      });
      if (!res.ok) throw new Error(`hub answered ${res.status}`);
      return (await res.json()) as T;
    } finally {
      clearTimeout(timer);
    }
  }

  state() {
    return this.call<HubState>('GET', '/api/state');
  }
  history(robot: string) {
    return this.call<HistorySample[]>('GET', `/api/history?id=${encodeURIComponent(robot)}`);
  }
  setDevice(id: string, patch: { on?: boolean; level?: number }) {
    return this.call<{ ok: boolean }>('POST', `/api/devices/${encodeURIComponent(id)}`, patch);
  }
  setRobot(robot: string, cfg: RobotCfg) {
    return this.call<{ ok: boolean }>('POST', `/api/robots/${encodeURIComponent(robot)}/cfg`, cfg);
  }
}

/** "192.168.1.20:8080" or "http://host:8080/" -> "http://host:8080" */
export function normalizeHubUrl(input: string): string {
  let s = input.trim().replace(/\/+$/, '');
  if (!s) return '';
  if (!/^https?:\/\//i.test(s)) s = `http://${s}`;
  return s;
}

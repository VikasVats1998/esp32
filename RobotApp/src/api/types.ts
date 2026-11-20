// Types for the robot HTTP API (see docs/API.md). Both robots speak the same API (v1);
// `caps` tells the app which extra features a robot has.
export type RobotKind = 'chiku' | 'jinu';
export type Mode = 'auto' | 'manual' | 'explore';

export interface ServoInfo {
  trim: number;
  min: number;
  max: number;
  inv: boolean;
  /** last commanded angle */
  cur: number;
}

export interface RobotState {
  robot: RobotKind;
  api: number;
  fw: string;
  caps: string[];
  name: string;
  ip: string;
  ssid: string;
  rssi: number;
  uptime: number;
  heap: number;
  mode: Mode;
  mute: boolean;
  mood: string;
  brightness: number;
  volume: number;
  /** filtered sonar distance in cm, -1 = no echo */
  dist: number;
  /** bit n = pad n is touched right now */
  pads: number;
  padsOnline: boolean;
  avoid: boolean;
  gait: string;
  busy: boolean;
  /** 0 active, 1 dim, 2 sleep */
  power: number;
  audio: boolean;
  servo: ServoInfo[];
  // ---- Chiku only (check `caps`) ----
  screen?: number;
  batt?: number;
  charging?: boolean;
  pca?: boolean;
  psram?: number;
  lat?: number;
  lon?: number;
  imu?: { ok: boolean; x: number; y: number; z: number; tilt: number };
  cam?: { alive: boolean; ip: string; state: string; stream: string };
  weather?: { valid: boolean; temp: number; min: number; max: number; text: string };
  alarms?: { on: boolean; h: number; m: number; days: number }[];
  timers?: number[];
}

export interface RobotEvent {
  n: number;
  /** robot uptime in ms when it happened */
  t: number;
  type: string;
  a: number;
  b: number;
}

export interface EventsReply {
  next: number;
  events: RobotEvent[];
}

/** A robot saved in the app. `host` may include a port (for the mock server). */
export interface SavedRobot {
  id: string;
  name: string;
  host: string;
  kind: RobotKind;
}

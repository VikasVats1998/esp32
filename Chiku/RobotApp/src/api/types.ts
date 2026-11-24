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
  screenName?: string;   // Chiku: name of the screen shown
  rotation?: number;     // Chiku: screen rotation in degrees
  setupDone?: boolean;
  mic?: number;          // Chiku: sound level dB (-1 = no mic)
  micOk?: boolean;
  batt?: number;
  charging?: boolean;
  pca?: boolean;
  psram?: number;
  lat?: number;
  lon?: number;
  imu?: { ok: boolean; x: number; y: number; z: number; tilt: number };
  cam?: { alive: boolean; ip: string; state: string; stream: string; snap?: number };
  weather?: { valid: boolean; temp: number; min: number; max: number; text: string };
  alarms?: { on: boolean; h: number; m: number; days: number }[];
  timers?: number[];
  /** the robot's own link to the standing desk (secure DeskLink); present when caps has "desk" */
  desk?: RobotDesk;
}

export interface RobotDesk {
  paired: boolean;
  host: string;
  ok: boolean;
  hv: boolean;
  h: number;
  moving: boolean;
  dir: number;
  armed: boolean;
  travel: boolean;
  src: string;
  err: string;
  /** ms since the last good answer, -1 = never */
  age: number;
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

/** One line of the robot's own log (serial output + system logs). l: 0 error, 1 warning, 2 info, 3 debug. */
export interface LogLine {
  n: number;
  t: number;
  l: number;
  m: string;
}

export interface LogReply {
  next: number;
  lines: LogLine[];
}

export interface DiagItem {
  name: string;
  ok: boolean;
  detail: string;
  /** true = measured now, false = result of the start-up check */
  live?: boolean;
}

export interface DiagReply {
  items: DiagItem[];
  heap?: number;
  uptime?: number;
}

/** Answer of a POST command. `msg` is the robot's own feedback text. */
export interface CommandReply {
  ok: boolean;
  msg?: string;
}

/** One request the app sent to a robot (for the Traffic log and the feedback line). */
export interface Traffic {
  id: number;
  at: number;
  host: string;
  method: string;
  path: string;
  body?: string;
  status?: number;
  ok: boolean;
  ms: number;
  response?: string;
  error?: string;
}

// In-memory logs and results (not persisted): robot log, request traffic, last feedback,
// hardware checklist results.
import { create } from 'zustand';

import type { LogLine, Traffic } from '@/api/types';

export interface LogEntry extends LogLine {
  /** true = written by the app (e.g. "robot restarted"), not by the robot */
  local?: boolean;
}

interface LogStore {
  lines: LogEntry[];
  cursor: number;
  push: (lines: LogLine[], next: number) => void;
  note: (text: string) => void;
  clear: () => void;
}

let localId = 0;

export const useLogs = create<LogStore>()((set) => ({
  lines: [],
  cursor: 0,
  push: (lines, next) =>
    set((s) => {
      // the robot's counter went back: it rebooted
      const rebooted = next < s.cursor;
      const marker: LogEntry[] = rebooted ? [{ n: --localId, t: 0, l: 1, m: '--- robot restarted ---', local: true }] : [];
      return { lines: [...s.lines, ...marker, ...lines].slice(-1500), cursor: next };
    }),
  note: (text) => set((s) => ({ lines: [...s.lines, { n: --localId, t: 0, l: 2, m: text, local: true }].slice(-1500) })),
  clear: () => set({ lines: [] }),
}));

interface TrafficStore {
  items: Traffic[];
  push: (t: Traffic) => void;
  clear: () => void;
}

export const useTraffic = create<TrafficStore>()((set) => ({
  items: [],
  push: (t) => set((s) => ({ items: [...s.items, t].slice(-500) })),
  clear: () => set({ items: [] }),
}));

export interface Feedback {
  id: number;
  ok: boolean;
  text: string;
  ms: number;
}

interface FeedbackStore {
  last?: Feedback;
  set: (f: Feedback) => void;
  clear: () => void;
}

export const useFeedback = create<FeedbackStore>()((set) => ({
  last: undefined,
  set: (last) => set({ last }),
  clear: () => set({ last: undefined }),
}));

export type TestStatus = 'running' | 'asking' | 'pass' | 'fail';

export interface TestResult {
  status: TestStatus;
  detail: string;
  at: number;
  /** question shown while status is 'asking' */
  ask?: string;
}

interface ChecklistStore {
  /** key = `${host}:${testId}` */
  results: Record<string, TestResult>;
  set: (key: string, r: TestResult) => void;
  clearHost: (host: string) => void;
}

export const useChecklist = create<ChecklistStore>()((set) => ({
  results: {},
  set: (key, r) => set((s) => ({ results: { ...s.results, [key]: r } })),
  clearHost: (host) =>
    set((s) => ({ results: Object.fromEntries(Object.entries(s.results).filter(([k]) => !k.startsWith(`${host}:`))) })),
}));

export interface ConsoleEntry {
  id: number;
  cmd: string;
  ok: boolean;
  text: string;
  ms: number;
}

interface ConsoleStore {
  entries: ConsoleEntry[];
  add: (e: Omit<ConsoleEntry, 'id'>) => void;
  clear: () => void;
}

let consoleId = 0;

export const useConsole = create<ConsoleStore>()((set) => ({
  entries: [],
  add: (e) => set((s) => ({ entries: [...s.entries, { ...e, id: ++consoleId }].slice(-100) })),
  clear: () => set({ entries: [] }),
}));

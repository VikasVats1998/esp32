// Saved robots (persisted) and small UI state (not persisted).
import AsyncStorage from '@react-native-async-storage/async-storage';
import { create } from 'zustand';
import { createJSONStorage, persist } from 'zustand/middleware';

import type { RobotEvent, SavedRobot } from '@/api/types';

interface RobotsStore {
  robots: SavedRobot[];
  activeId?: string;
  add: (r: Omit<SavedRobot, 'id'>) => string;
  remove: (id: string) => void;
  setActive: (id: string) => void;
}

export const useRobots = create<RobotsStore>()(
  persist(
    (set, get) => ({
      robots: [],
      activeId: undefined,
      add: (r) => {
        const existing = get().robots.find((x) => x.host === r.host);
        if (existing) {
          set({ activeId: existing.id });
          return existing.id;
        }
        const id = `${r.kind}-${Date.now().toString(36)}`;
        set((s) => ({ robots: [...s.robots, { ...r, id }], activeId: id }));
        return id;
      },
      remove: (id) =>
        set((s) => {
          const robots = s.robots.filter((r) => r.id !== id);
          return { robots, activeId: s.activeId === id ? robots[0]?.id : s.activeId };
        }),
      setActive: (id) => set({ activeId: id }),
    }),
    { name: 'robots-v1', storage: createJSONStorage(() => AsyncStorage) },
  ),
);

interface UiStore {
  error?: string;
  setError: (e?: string) => void;
  events: RobotEvent[];
  cursor: number;
  pushEvents: (events: RobotEvent[], next: number) => void;
  clearEvents: () => void;
}

export const useUi = create<UiStore>()((set) => ({
  error: undefined,
  setError: (error) => set({ error }),
  events: [],
  cursor: 0,
  pushEvents: (events, next) =>
    set((s) => ({
      // a robot that rebooted starts counting again from 0: start a fresh log
      events: next < s.cursor ? events.slice(-200) : [...s.events, ...events].slice(-200),
      cursor: next,
    })),
  clearEvents: () => set({ events: [] }),
}));

// Where the hub server is (address + token), kept in AsyncStorage.
import AsyncStorage from '@react-native-async-storage/async-storage';
import { create } from 'zustand';

import { normalizeHubUrl } from '@/hub/client';

const KEY = 'hub-v1';

interface HubStore {
  url: string;
  token: string;
  loaded: boolean;
  load: () => Promise<void>;
  save: (url: string, token: string) => Promise<void>;
}

export const useHub = create<HubStore>()((set) => ({
  url: '',
  token: 'chiku',
  loaded: false,
  load: async () => {
    try {
      const raw = await AsyncStorage.getItem(KEY);
      if (raw) {
        const v = JSON.parse(raw) as { url: string; token: string };
        set({ url: v.url ?? '', token: v.token || 'chiku', loaded: true });
        return;
      }
    } catch {
      /* nothing saved yet */
    }
    set({ loaded: true });
  },
  save: async (url, token) => {
    const clean = normalizeHubUrl(url);
    const tok = token.trim() || 'chiku';
    set({ url: clean, token: tok });
    await AsyncStorage.setItem(KEY, JSON.stringify({ url: clean, token: tok })).catch(() => {});
  },
}));

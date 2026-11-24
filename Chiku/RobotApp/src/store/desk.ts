// The paired desk module: host and client id are in AsyncStorage, the secret key only in the
// device's secure storage (Keychain / Keystore).
import AsyncStorage from '@react-native-async-storage/async-storage';
import * as SecureStore from 'expo-secure-store';
import { create } from 'zustand';

import { DeskLinkClient, type DeskCreds } from '@/desk/client';
import { randomHex } from '@/desk/crypto';

const KEY_NAME = 'desk-key-v1';
const META_NAME = 'desk-meta-v1';
const TOKEN_NAME = 'robot-desk-token-v1';

interface DeskStore {
  host?: string;
  id?: string;
  key?: string;
  /** secret that lets this phone use the desk endpoints of the robots (created once) */
  token?: string;
  loaded: boolean;
  load: () => Promise<void>;
  /** Pairs with the desk at `host` and saves the result. */
  pair: (host: string, appName: string) => Promise<void>;
  forget: () => Promise<void>;
}

export const useDesk = create<DeskStore>()((set) => ({
  loaded: false,
  load: async () => {
    try {
      let token = await SecureStore.getItemAsync(TOKEN_NAME);
      if (!token) {
        token = randomHex(16);
        await SecureStore.setItemAsync(TOKEN_NAME, token);
      }
      set({ token });
    } catch {
      /* secure storage not available */
    }
    try {
      const meta = await AsyncStorage.getItem(META_NAME);
      const key = await SecureStore.getItemAsync(KEY_NAME);
      if (meta && key) {
        const m = JSON.parse(meta) as { host: string; id: string };
        set({ host: m.host, id: m.id, key, loaded: true });
        return;
      }
    } catch {
      /* nothing saved yet */
    }
    set({ loaded: true });
  },
  pair: async (host, appName) => {
    const creds = await new DeskLinkClient(host).pair(appName);
    await SecureStore.setItemAsync(KEY_NAME, creds.key);
    await AsyncStorage.setItem(META_NAME, JSON.stringify({ host, id: creds.id }));
    set({ host, id: creds.id, key: creds.key });
  },
  forget: async () => {
    await SecureStore.deleteItemAsync(KEY_NAME).catch(() => {});
    await AsyncStorage.removeItem(META_NAME).catch(() => {});
    set({ host: undefined, id: undefined, key: undefined });
  },
}));

/** The client for the paired desk, or undefined. */
export function deskClientFrom(s: { host?: string; id?: string; key?: string }): DeskLinkClient | undefined {
  if (!s.host || !s.id || !s.key) return undefined;
  const creds: DeskCreds = { id: s.id, key: s.key };
  return new DeskLinkClient(s.host, creds);
}

export const getDeskClient = () => deskClientFrom(useDesk.getState());

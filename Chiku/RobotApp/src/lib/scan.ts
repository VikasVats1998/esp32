// Finds robots on the local network: asks every address of the phone's /24 subnet
// for /api/state. Works everywhere (no mDNS needed) and in Expo Go.
import * as Network from 'expo-network';

import type { RobotState } from '@/api/types';

export interface Found {
  host: string;
  state: RobotState;
}

async function probe(host: string, timeoutMs: number): Promise<Found | undefined> {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const res = await fetch(`http://${host}/api/state`, { signal: ctrl.signal });
    if (!res.ok) return undefined;
    const state = (await res.json()) as RobotState;
    return state && (state.robot === 'chiku' || state.robot === 'jinu') ? { host, state } : undefined;
  } catch {
    return undefined;
  } finally {
    clearTimeout(timer);
  }
}

export async function scanNetwork(onProgress?: (done: number, total: number) => void): Promise<Found[]> {
  const ip = await Network.getIpAddressAsync();
  const m = /^(\d+\.\d+\.\d+)\.\d+$/.exec(ip);
  if (!m) throw new Error(`Phone has no IPv4 Wi-Fi address (got "${ip}")`);
  const hosts = Array.from({ length: 254 }, (_, i) => `${m[1]}.${i + 1}`);
  const found: Found[] = [];
  let done = 0;
  const queue = [...hosts];
  const worker = async () => {
    for (let h = queue.shift(); h; h = queue.shift()) {
      const f = await probe(h, 700);
      if (f) found.push(f);
      onProgress?.(++done, hosts.length);
    }
  };
  await Promise.all(Array.from({ length: 32 }, worker));
  return found;
}

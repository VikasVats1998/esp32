// Secure client for the desk module (DeskLink v1). Plain TypeScript (works in React Native and Node).
// Every command is two requests: GET /dl/challenge, then POST /dl/cmd with the command sealed with AES-GCM
// (challenge = AAD, one use). The answer is sealed too and bound to the same challenge.
import { RobotClient } from '@/api/client';
import type { Traffic } from '@/api/types';

import { deriveKey, fromHex, generateKeyPair, open, seal, toHex } from './crypto';

export class DeskError extends Error {
  constructor(message: string, readonly status?: number) {
    super(message);
    this.name = 'DeskError';
  }
}

export interface DeskCreds {
  id: string;
  /** 32 byte AES key (hex) */
  key: string;
}

export interface DeskInfo {
  dev: string;
  fw: string;
  link: number;
  pairing: boolean;
  left: number;
  clients: number;
  locked: boolean;
}

export interface DeskState {
  ok: boolean;
  err?: string;
  /** height in cm (0 = unknown) */
  h: number;
  hv: boolean;
  /** "display" (read from the handset cable), "estimate" or "none" */
  src: string;
  moving: boolean;
  dir: number;
  travel: boolean;
  disp: string;
  armed: boolean;
  phold: boolean;
  /** one char per key (up, down, 1-4, s, x): # = pressed */
  keys: string;
  frames: number;
  bad: number;
  bytes: number;
  up: number;
  rssi: number;
  fw: string;
}

export interface DeskLine {
  n: number;
  t: number;
  l: number;
  m: string;
}

let nextId = 1_000_000;

export class DeskLinkClient {
  constructor(
    readonly host: string,
    private readonly creds?: DeskCreds,
    private readonly timeoutMs = 3000,
  ) {}

  private async http(method: 'GET' | 'POST', path: string, body: string | undefined, label: string, quiet = false) {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), this.timeoutMs);
    const started = Date.now();
    const report = (t: Partial<Traffic>) => {
      if (quiet || !RobotClient.onTraffic) return;
      RobotClient.onTraffic({ id: ++nextId, at: started, host: this.host, method, path: label, ok: false, ms: Date.now() - started, ...t });
    };
    try {
      const res = await fetch(`http://${this.host}${path}`, {
        method,
        body,
        headers: body ? { 'Content-Type': 'application/json' } : undefined,
        signal: ctrl.signal,
      });
      const text = await res.text().catch(() => '');
      return { status: res.status, text, report, ms: Date.now() - started };
    } catch (e) {
      const message = ctrl.signal.aborted ? `${this.host}: no answer (timeout)` : `${this.host}: ${(e as Error).message}`;
      report({ error: message });
      throw new DeskError(message);
    } finally {
      clearTimeout(timer);
    }
  }

  async info(): Promise<DeskInfo> {
    const r = await this.http('GET', '/dl/info', undefined, 'desk /dl/info', true);
    if (r.status !== 200) throw new DeskError(`desk info: HTTP ${r.status}`, r.status);
    return JSON.parse(r.text) as DeskInfo;
  }

  /** Pairs this app with the desk (its pairing window must be open: power-cycle the desk). */
  async pair(name: string): Promise<DeskCreds> {
    const kp = generateKeyPair();
    const r = await this.http('POST', '/dl/pair', JSON.stringify({ name, pub: toHex(kp.pub) }), 'desk pair');
    if (r.status !== 200) {
      r.report({ status: r.status, ok: false, response: r.text });
      throw new DeskError(r.status === 403 ? 'Pairing is closed. Power-cycle the desk, then pair within 3 minutes.' : `pairing failed: ${r.text || r.status}`, r.status);
    }
    const j = JSON.parse(r.text) as { id: string; pub: string };
    const key = deriveKey(kp, fromHex(j.pub), kp.pub, fromHex(j.pub));
    r.report({ status: 200, ok: true, response: `paired as ${j.id}` });
    return { id: j.id, key: toHex(key) };
  }

  /** Sends one sealed command and returns the opened answer. `quiet` = polling (not shown in Requests). */
  async call<T extends { ok: boolean; err?: string }>(command: Record<string, unknown>, quiet = false): Promise<T> {
    if (!this.creds) throw new DeskError('not paired');
    const key = fromHex(this.creds.key);
    const label = `desk ${String(command.op)}${command.dir ? ` ${String(command.dir)}` : ''}${command.n ? ` ${String(command.n)}` : ''}${command.key ? ` ${String(command.key)}` : ''}`;
    const c = await this.http('GET', '/dl/challenge', undefined, label, quiet);
    if (c.status !== 200) {
      c.report({ status: c.status, ok: false, response: c.text });
      throw new DeskError(`challenge failed: HTTP ${c.status}`, c.status);
    }
    const challenge = fromHex((JSON.parse(c.text) as { c: string }).c);
    const sealed = seal(key, challenge, 'cmd', JSON.stringify(command));
    const r = await this.http('POST', '/dl/cmd', JSON.stringify({ id: this.creds.id, c: toHex(challenge), d: toHex(sealed) }), label, quiet);
    if (r.status === 401) {
      r.report({ status: 401, ok: false, error: 'desk rejected the key: pair again' });
      throw new DeskError('The desk rejected our key. Pair again (power-cycle the desk first).', 401);
    }
    if (r.status === 429) {
      r.report({ status: 429, ok: false, error: 'locked' });
      throw new DeskError('The desk is locked for a minute after too many bad messages.', 429);
    }
    if (r.status !== 200) {
      r.report({ status: r.status, ok: false, response: r.text });
      throw new DeskError(`desk error: HTTP ${r.status}`, r.status);
    }
    let answer: string;
    try {
      answer = open(key, challenge, 'rsp', fromHex((JSON.parse(r.text) as { d: string }).d));
    } catch {
      r.report({ status: 200, ok: false, error: 'answer could not be verified' });
      throw new DeskError('The desk answer could not be verified (wrong key or tampered).');
    }
    const reply = JSON.parse(answer) as T;
    r.report({ status: 200, ok: reply.ok, response: answer.slice(0, 300), error: reply.ok ? undefined : reply.err });
    return reply;
  }

  /**
   * Security self test (costs the desk 2 of its 5 allowed bad messages per minute):
   * a recorded request must not work twice, and a message sealed with a wrong key must be rejected.
   */
  async securityProbe(): Promise<{ replayRejected: boolean; wrongKeyRejected: boolean }> {
    if (!this.creds) throw new DeskError('not paired');
    const key = fromHex(this.creds.key);
    const raw = (path: string, body?: string) => fetch(`http://${this.host}${path}`, { method: body ? 'POST' : 'GET', body, headers: body ? { 'Content-Type': 'application/json' } : undefined });
    const challenge = async () => ((await (await raw('/dl/challenge')).json()) as { c: string }).c;
    const c1 = await challenge();
    const req = JSON.stringify({ id: this.creds.id, c: c1, d: toHex(seal(key, fromHex(c1), 'cmd', JSON.stringify({ op: 'state' }))) });
    const first = await raw('/dl/cmd', req);
    const second = await raw('/dl/cmd', req);
    const evil = generateKeyPair();
    const evilKey = deriveKey(evil, evil.pub, evil.pub, evil.pub);
    const c2 = await challenge();
    const bad = await raw('/dl/cmd', JSON.stringify({ id: this.creds.id, c: c2, d: toHex(seal(evilKey, fromHex(c2), 'cmd', JSON.stringify({ op: 'state' }))) }));
    return { replayRejected: first.status === 200 && second.status === 401, wrongKeyRejected: bad.status === 401 };
  }

  // ---- convenience ----
  state(quiet = true) {
    return this.call<DeskState>({ op: 'state' }, quiet);
  }
  /** Hold UP/DOWN for `ms` (the desk releases itself; call again every 250 ms to keep moving). */
  move(dir: 'up' | 'down', ms = 700) {
    return this.call<{ ok: boolean; err?: string }>({ op: 'move', dir, ms }, true);
  }
  stop() {
    return this.call<{ ok: boolean; err?: string }>({ op: 'stop' });
  }
  preset(n: number) {
    return this.call<{ ok: boolean; err?: string }>({ op: 'preset', n });
  }
  saveMemory(n: number) {
    return this.call<{ ok: boolean; err?: string }>({ op: 'save', n });
  }
  tap(key: string, ms = 150) {
    return this.call<{ ok: boolean; err?: string }>({ op: 'tap', key, ms });
  }
  setHeight(h: number) {
    return this.call<{ ok: boolean; err?: string }>({ op: 'setheight', h });
  }
  arm(on: boolean) {
    return this.call<{ ok: boolean; err?: string }>({ op: 'arm', on });
  }
  setPresetHold(on: boolean) {
    return this.call<{ ok: boolean; err?: string }>({ op: 'cfg', presetHold: on });
  }
  log(since: number) {
    return this.call<{ ok: boolean; next: number; lines: DeskLine[] }>({ op: 'log', since });
  }
  raw() {
    return this.call<{ ok: boolean; text: string }>({ op: 'raw' });
  }
  clients() {
    return this.call<{ ok: boolean; clients: { id: string; name: string; you: boolean }[] }>({ op: 'clients' });
  }
  unpair() {
    return this.call<{ ok: boolean; err?: string }>({ op: 'unpair' });
  }
}

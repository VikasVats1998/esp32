// The `desk ...` console commands (talk to the desk module directly, no robot needed).
import { normaliseHost } from '@/api/client';
import type { CommandResult } from '@/lib/commands';
import { getDeskClient, useDesk } from '@/store/desk';

const num = (s: string | undefined, d?: number) => {
  const n = Number(s);
  if (s === undefined || Number.isNaN(n)) {
    if (d !== undefined) return d;
    throw new Error(`"${s ?? ''}" is not a number`);
  }
  return n;
};

export async function runDesk(a: string[]): Promise<CommandResult> {
  const sub = (a[0] ?? 'state').toLowerCase();
  if (sub === 'pair') {
    const host = normaliseHost(a[1] ?? 'desk.local');
    await useDesk.getState().pair(host, 'phone app');
    return { ok: true, text: `paired with ${host}` };
  }
  const d = getDeskClient();
  if (!d) throw new Error('no desk paired: desk pair <host> (power-cycle the desk first)');
  const done = (r: { ok: boolean; err?: string }) => ({ ok: r.ok, text: r.ok ? 'ok' : `refused: ${r.err ?? '?'}` });
  switch (sub) {
    case 'state': return { ok: true, text: JSON.stringify(await d.state(false), null, 1) };
    case 'up': return done(await d.move('up', 500));
    case 'down': return done(await d.move('down', 500));
    case 'stop': return done(await d.stop());
    case 'preset': return done(await d.preset(num(a[1])));
    case 'save': return done(await d.saveMemory(num(a[1])));
    case 'tap': return done(await d.tap(a[1] ?? '', num(a[2], 150)));
    case 'height': return done(await d.setHeight(num(a[1])));
    case 'arm': return done(await d.arm(a[1] !== 'off' && a[1] !== '0'));
    case 'log': { const r = await d.log(0); return { ok: r.ok, text: r.lines.slice(-30).map((l) => `${(l.t / 1000).toFixed(1)}s ${l.m}`).join('\n') }; }
    case 'raw': { const r = await d.raw(); return { ok: r.ok, text: r.text || '(nothing captured)' }; }
    case 'clients': { const r = await d.clients(); return { ok: r.ok, text: r.clients.map((c) => `${c.id} ${c.name}${c.you ? ' (this phone)' : ''}`).join('\n') }; }
    case 'forget': { await d.unpair().catch(() => {}); await useDesk.getState().forget(); return { ok: true, text: 'forgotten' }; }
    default: throw new Error('desk state|up|down|stop|preset|save|tap|height|arm|log|raw|clients|pair|forget');
  }
}


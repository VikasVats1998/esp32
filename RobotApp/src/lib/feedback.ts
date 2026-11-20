// Turns every request into a one-line feedback message and a Traffic entry.
import { RobotClient } from '@/api/client';
import type { Traffic } from '@/api/types';
import { useFeedback, useTraffic } from '@/store/diagnostics';

function robotMessage(t: Traffic): string {
  if (!t.response) return '';
  try {
    const j = JSON.parse(t.response) as { msg?: string };
    return j.msg ? ` · ${j.msg}` : '';
  } catch {
    return '';
  }
}

/** Call once at start-up. */
export function installFeedback() {
  RobotClient.onTraffic = (t) => {
    useTraffic.getState().push(t);
    const short = t.path.length > 44 ? `${t.path.slice(0, 44)}...` : t.path;
    useFeedback.getState().set({
      id: t.id,
      ok: t.ok,
      ms: t.ms,
      text: t.ok ? `${t.method} ${short}${robotMessage(t)}` : `${t.method} ${short}: ${t.error ?? `HTTP ${t.status} ${t.response ?? ''}`}`,
    });
  };
}

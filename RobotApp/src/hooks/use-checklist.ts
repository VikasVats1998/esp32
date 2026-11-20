// Runs the hardware checklist tests (src/lib/checklist.ts) and keeps their results.
import { useCallback, useMemo } from 'react';

import { type Ctx, type TestDef, buildDeskTests, buildTests, wait } from '@/lib/checklist';
import type { RobotClient } from '@/api/client';
import { useChecklist, useLogs, type TestResult } from '@/store/diagnostics';

import { useDeskClient } from './use-desk';
import { useActiveRobot, useRobotState } from './use-robot';

export function useChecklistRunner() {
  const { robot, client } = useActiveRobot();
  const { state } = useRobotState();
  const results = useChecklist((s) => s.results);
  const setResult = useChecklist((s) => s.set);
  const clearHost = useChecklist((s) => s.clearHost);
  const kind = robot?.kind ?? 'chiku';
  const host = robot?.host ?? '';

  const desk = useDeskClient();
  const tests = useMemo<TestDef[]>(() => [...buildTests(kind), ...(desk ? buildDeskTests() : [])], [kind, desk]);

  const resultOf = useCallback((id: string): TestResult | undefined => results[`${host}:${id}`], [results, host]);

  const run = useCallback(
    async (t: TestDef) => {
      const isDesk = t.group === 'Desk';
      if (isDesk ? !desk : !client) return;
      const key = `${host}:${t.id}`;
      const ctx: Ctx = {
        client: client as RobotClient, // desk tests never use it
        desk,
        kind,
        sleep: wait,
        note: (text) => setResult(key, { status: 'running', detail: text, at: Date.now() }),
        errorsSince: () => useLogs.getState().lines.filter((l) => !l.local && l.l === 0).length,
      };
      setResult(key, { status: 'running', detail: t.help, at: Date.now() });
      try {
        const out = await t.run(ctx);
        if ('ask' in out) setResult(key, { status: 'asking', detail: out.detail ?? 'done, waiting for your answer', ask: out.ask, at: Date.now() });
        else setResult(key, { status: out.ok ? 'pass' : 'fail', detail: out.detail, at: Date.now() });
      } catch (e) {
        setResult(key, { status: 'fail', detail: (e as Error).message, at: Date.now() });
      }
    },
    [client, desk, host, kind, setResult],
  );

  const answer = useCallback(
    (t: TestDef, ok: boolean) => {
      const prev = results[`${host}:${t.id}`];
      setResult(`${host}:${t.id}`, { status: ok ? 'pass' : 'fail', detail: ok ? 'confirmed by you' : 'you said: did not work', at: Date.now(), ask: prev?.ask });
    },
    [host, results, setResult],
  );

  const runAuto = useCallback(async () => {
    for (const t of tests.filter((x) => x.kind === 'auto' && x.id !== 'log_clean')) await run(t);
    const clean = tests.find((x) => x.id === 'log_clean');
    if (clean) await run(clean);
  }, [tests, run]);

  const report = useCallback(() => {
    const lines = [
      `Hardware test report: ${robot?.name ?? '?'} (${robot?.host ?? '?'})`,
      `Firmware ${state?.fw ?? '?'}, uptime ${state?.uptime ?? '?'} s, ${new Date().toISOString()}`,
      '',
    ];
    let group = '';
    for (const t of tests) {
      if (t.group !== group) {
        group = t.group;
        lines.push(`== ${group}`);
      }
      const r = results[`${host}:${t.id}`];
      const mark = !r ? 'NOT RUN' : r.status === 'pass' ? 'PASS' : r.status === 'fail' ? 'FAIL' : r.status.toUpperCase();
      lines.push(`${mark.padEnd(8)} ${t.title}${r && r.detail && r.status !== 'running' ? ` - ${r.detail}` : ''}`);
    }
    return lines.join('\n');
  }, [robot, state, tests, results, host]);

  return { tests, resultOf, run, answer, runAuto, report, reset: () => clearHost(host), enabled: !!client || !!desk };
}

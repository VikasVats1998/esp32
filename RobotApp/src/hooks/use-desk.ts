// Hooks for the desk module (talks to it directly from the phone through the secure link).
import { useQuery } from '@tanstack/react-query';
import * as Haptics from 'expo-haptics';
import { useCallback, useMemo } from 'react';

import type { DeskLinkClient, DeskState } from '@/desk/client';
import { deskClientFrom, useDesk } from '@/store/desk';
import { useUi } from '@/store/robots';

export function useDeskClient(): DeskLinkClient | undefined {
  const host = useDesk((s) => s.host);
  const id = useDesk((s) => s.id);
  const key = useDesk((s) => s.key);
  return useMemo(() => deskClientFrom({ host, id, key }), [host, id, key]);
}

/** Desk state, polled every second while `enabled`. */
export function useDeskState(enabled = true) {
  const client = useDeskClient();
  const host = useDesk((s) => s.host);
  const q = useQuery<DeskState>({
    queryKey: ['desk', host],
    queryFn: () => client!.state(true),
    enabled: !!client && enabled,
    refetchInterval: 1000,
    retry: false,
  });
  return { client, state: q.data, connected: !!q.data && !q.isError, error: q.error as Error | null, refetch: q.refetch };
}

/** Runs a desk command: haptic tap, error banner when it fails or the desk refuses. */
export function useDeskSend() {
  const { client, refetch } = useDeskState(false);
  const setError = useUi((s) => s.setError);
  return useCallback(
    async (fn: (c: DeskLinkClient) => Promise<{ ok: boolean; err?: string }>) => {
      if (!client) {
        setError('No desk paired. Open Control, Desk and pair first.');
        return false;
      }
      try {
        Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light).catch(() => {});
        const r = await fn(client);
        if (!r.ok) {
          setError(`Desk refused: ${r.err ?? 'unknown reason'}`);
          return false;
        }
        setError(undefined);
        refetch();
        return true;
      } catch (e) {
        setError((e as Error).message);
        return false;
      }
    },
    [client, refetch, setError],
  );
}

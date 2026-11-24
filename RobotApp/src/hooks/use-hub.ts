// Hooks for the hub server: live state and sensor history, plus device / robot changes.
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import * as Haptics from 'expo-haptics';
import { useMemo } from 'react';

import { HubClient, type HistorySample, type HubState, type RobotCfg } from '@/hub/client';
import { useHub } from '@/store/hub';

export function useHubClient(): HubClient | undefined {
  const url = useHub((s) => s.url);
  const token = useHub((s) => s.token);
  return useMemo(() => (url ? new HubClient(url, token) : undefined), [url, token]);
}

/** Hub state, polled every second. */
export function useHubState() {
  const client = useHubClient();
  const q = useQuery<HubState>({
    queryKey: ['hub', 'state', client?.base],
    queryFn: () => client!.state(),
    enabled: !!client,
    refetchInterval: 1000,
    retry: false,
  });
  const robotId = useMemo(() => {
    const ids = Object.keys(q.data?.robots ?? {});
    return ids.includes('chiku') ? 'chiku' : ids[0];
  }, [q.data]);
  return { client, state: q.data, robotId, online: !!q.data && !q.isError, error: q.error as Error | null };
}

/** Last ~10 minutes of sensor samples (one per second), refreshed every 3 s. */
export function useHubHistory(robotId: string | undefined) {
  const client = useHubClient();
  return useQuery<HistorySample[]>({
    queryKey: ['hub', 'history', client?.base, robotId],
    queryFn: () => client!.history(robotId!),
    enabled: !!client && !!robotId,
    refetchInterval: 3000,
    retry: false,
  });
}

export function useHubActions() {
  const client = useHubClient();
  const qc = useQueryClient();
  const refresh = () => qc.invalidateQueries({ queryKey: ['hub', 'state'] });
  const device = useMutation({
    mutationFn: async (v: { id: string; on?: boolean; level?: number }) => {
      Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light).catch(() => {});
      return client!.setDevice(v.id, { on: v.on, level: v.level });
    },
    onSuccess: refresh,
  });
  const robot = useMutation({
    mutationFn: async (v: { robot: string; cfg: RobotCfg }) => client!.setRobot(v.robot, v.cfg),
    onSuccess: refresh,
  });
  return { device, robot };
}

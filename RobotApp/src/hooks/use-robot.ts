// Hooks that give screens the active robot, its live state and a safe way to send commands.
import { useQuery } from '@tanstack/react-query';
import * as Haptics from 'expo-haptics';
import { useCallback, useEffect, useMemo } from 'react';

import { RobotClient } from '@/api/client';
import type { RobotState, SavedRobot } from '@/api/types';
import { useRobots, useUi } from '@/store/robots';

export function useActiveRobot(): { robot?: SavedRobot; client?: RobotClient } {
  const robots = useRobots((s) => s.robots);
  const activeId = useRobots((s) => s.activeId);
  const robot = robots.find((r) => r.id === activeId);
  const client = useMemo(() => (robot ? new RobotClient(robot.host) : undefined), [robot]);
  return { robot, client };
}

/** Live robot state, polled about once a second. */
export function useRobotState() {
  const { robot, client } = useActiveRobot();
  const query = useQuery<RobotState>({
    queryKey: ['state', robot?.host],
    queryFn: () => client!.state(),
    enabled: !!client,
    refetchInterval: 1000,
    retry: false,
  });
  return {
    robot,
    client,
    state: query.data,
    connected: !!query.data && !query.isError,
    error: query.error as Error | null,
    refetch: query.refetch,
  };
}

/** Polls the robot's event history (touch gestures, shakes ...) into the log. */
export function useEventPump() {
  const { client, connected } = useRobotState();
  const pushEvents = useUi((s) => s.pushEvents);
  useEffect(() => {
    if (!client || !connected) return;
    let stopped = false;
    const tick = async () => {
      try {
        const since = useUi.getState().cursor;
        const reply = await client.events(since);
        if (!stopped && (reply.events.length || reply.next < since)) pushEvents(reply.events, reply.next);
      } catch {
        /* the state query already shows the connection error */
      }
    };
    const id = setInterval(tick, 500);
    return () => {
      stopped = true;
      clearInterval(id);
    };
  }, [client, connected, pushEvents]);
}

/** Wraps a robot call: haptic tap, error banner on failure, state refresh afterwards. */
export function useSend() {
  const { client, refetch } = useRobotState();
  const setError = useUi((s) => s.setError);
  return useCallback(
    async (fn: (c: RobotClient) => Promise<unknown>) => {
      if (!client) {
        setError('No robot selected. Add one on the Robots tab.');
        return false;
      }
      try {
        Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light).catch(() => {});
        await fn(client);
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

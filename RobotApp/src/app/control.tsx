import { useEffect, useState } from 'react';
import { GestureResponderEvent, Pressable, StyleSheet, View } from 'react-native';

import { EMOTES, MOODS, MOVES, SCREENS } from '@/api/catalog';
import type { Mode } from '@/api/types';
import { ThemedText } from '@/components/themed-text';
import { Button, Card, Chip, Page, Segmented, SwitchRow, Wrap } from '@/components/ui';
import { useRobotState, useSend } from '@/hooks/use-robot';
import { useTheme } from '@/hooks/use-theme';

const MODE_HELP: Record<Mode, string> = {
  auto: 'Normal personality: reacts to touch, sound of hands, has feelings and idle life.',
  manual: 'For testing: no automatic reactions, no idle life, never sleeps. Only your commands act.',
  explore: 'Walks around and avoids obstacles with the sonar, still reacts to touch.',
};

export default function ControlScreen() {
  const theme = useTheme();
  const send = useSend();
  const { state, client } = useRobotState();
  const [hold, setHold] = useState(false);
  const [voices, setVoices] = useState<string[]>([]);
  const isChiku = state?.robot === 'chiku';

  useEffect(() => {
    client?.voiceList().then(setVoices).catch(() => setVoices([]));
  }, [client]);

  const onLook = (e: GestureResponderEvent) => {
    const { locationX, locationY } = e.nativeEvent;
    const size = 220;
    const x = Math.max(-1, Math.min(1, (locationX - size / 2) / (size / 2)));
    const y = Math.max(-1, Math.min(1, (locationY - size / 2) / (size / 2)));
    send((c) => c.look(x, y));
  };

  return (
    <Page title="Control">
      <Card title="Mode">
        <Segmented<Mode>
          options={[
            { value: 'auto', label: 'Auto' },
            { value: 'manual', label: 'Manual' },
            { value: 'explore', label: 'Explore' },
          ]}
          value={state?.mode ?? 'auto'}
          onChange={(m) => send((c) => c.setMode(m))}
        />
        <ThemedText type="small" themeColor="textSecondary">
          {MODE_HELP[state?.mode ?? 'auto']}
        </ThemedText>
        <SwitchRow label="Mute" hint="Silences all sounds and voice" value={!!state?.mute} onChange={(v) => send((c) => c.setMute(v))} />
      </Card>

      <Card title="Faces">
        <SwitchRow label="Hold the face" hint="Stay in the mood until you pick another (otherwise 5 s)" value={hold} onChange={setHold} />
        <Wrap>
          {MOODS.map((m, i) => (
            <Chip key={m} label={m} active={state?.mood === m} onPress={() => send((c) => c.mood(i, hold ? 0 : 5000))} />
          ))}
        </Wrap>
      </Card>

      <Card title="Look">
        <ThemedText type="small" themeColor="textSecondary">
          Touch the pad: the eyes (and the head, if the robot has a pan servo) look there.
        </ThemedText>
        <Pressable onPress={onLook} style={[styles.pad, { backgroundColor: theme.backgroundSelected }]}>
          <View style={[styles.crossH, { backgroundColor: theme.textSecondary }]} />
          <View style={[styles.crossV, { backgroundColor: theme.textSecondary }]} />
        </Pressable>
      </Card>

      <Card title="Feelings (face + body + voice)">
        <Wrap>
          {EMOTES.map((e) => (
            <Chip key={e} label={e} onPress={() => send((c) => c.motion(e, 3))} />
          ))}
        </Wrap>
      </Card>

      <Card title="Moves">
        <Wrap>
          {MOVES.map((m) => (
            <Chip key={m} label={m} active={state?.gait === m && state?.busy} onPress={() => send((c) => c.motion(m, 4))} />
          ))}
        </Wrap>
        <Button label="Stop moving" color="#FF3B30" onPress={() => send((c) => c.stopMotion())} />
      </Card>

      <Card title="Voice">
        <Wrap>
          {voices.length === 0 ? <ThemedText themeColor="textSecondary">No voice clips found (run uploadfs). Babble is used instead.</ThemedText> : null}
          {voices.map((v) => (
            <Chip key={v} label={v} onPress={() => send((c) => c.say(v))} />
          ))}
        </Wrap>
      </Card>

      {isChiku ? (
        <Card title="Screen on Chiku">
          <Wrap>
            {SCREENS.map((s, i) => (
              <Chip key={s} label={s} active={state?.screen === i} onPress={() => send((c) => c.screen(i))} />
            ))}
          </Wrap>
        </Card>
      ) : null}
    </Page>
  );
}

const styles = StyleSheet.create({
  pad: { width: 220, height: 220, borderRadius: 24, alignSelf: 'center', justifyContent: 'center', alignItems: 'center' },
  crossH: { position: 'absolute', width: 200, height: 1, opacity: 0.4 },
  crossV: { position: 'absolute', height: 200, width: 1, opacity: 0.4 },
});

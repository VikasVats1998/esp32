import { useMemo, useState } from 'react';
import { StyleSheet, View } from 'react-native';

import { DISPLAY_PATTERNS, GESTURES, MELODIES, PAD_NAMES } from '@/api/catalog';
import { ThemedText } from '@/components/themed-text';
import { Checklist } from '@/components/checklist';
import { ServosPanel } from '@/components/servos-panel';
import { Button, Card, Chip, Page, Segmented, Stat, Wrap, useAccent } from '@/components/ui';
import { Spacing } from '@/constants/theme';
import { useRobotState, useSend } from '@/hooks/use-robot';
import { useTheme } from '@/hooks/use-theme';
import { useUi } from '@/store/robots';

export default function TestScreen() {
  const theme = useTheme();
  const accent = useAccent();
  const send = useSend();
  const { state } = useRobotState();
  const events = useUi((s) => s.events);
  const [simPad, setSimPad] = useState(0);
  const [tab, setTab] = useState<'live' | 'checklist' | 'servos' | 'simulate'>('live');

  const kind = state?.robot ?? 'chiku';
  const padNames = PAD_NAMES[kind];
  const isChiku = kind === 'chiku';
  const lastPad = useMemo(() => {
    const m: Record<number, string> = {};
    for (const e of events) if (e.type === 'pad') m[e.a] = GESTURES[e.b] ?? String(e.b);
    return m;
  }, [events]);

  const dist = state?.dist ?? -1;
  const distPct = dist < 0 ? 0 : Math.min(100, (dist / 200) * 100);
  const distColor = dist < 0 ? theme.textSecondary : dist < 10 ? '#FF3B30' : dist < 30 ? '#FF9F0A' : '#34C759';

  return (
    <Page title="Test">
      <Segmented
        options={[
          { value: 'live' as const, label: 'Live' },
          { value: 'checklist' as const, label: 'Checklist' },
          { value: 'servos' as const, label: 'Servos' },
          { value: 'simulate' as const, label: 'Simulate' },
        ]}
        value={tab}
        onChange={setTab}
      />
      {tab === 'checklist' ? <Checklist /> : null}
      {tab === 'servos' ? <ServosPanel /> : null}
      {tab === 'live' ? (
        <>
      <Card title="Touch pads (live)">
        {state && !state.padsOnline ? (
          <ThemedText type="small" style={{ color: '#FF3B30' }}>
            PCF8574 expander not found: check wiring on the pad bus (address 0x20).
          </ThemedText>
        ) : null}
        <Wrap>
          {padNames.map((name, i) => {
            const on = !!state && (state.pads & (1 << i)) !== 0;
            return (
              <View key={name} style={[styles.pad, { backgroundColor: on ? accent : theme.backgroundSelected }]}>
                <ThemedText type="smallBold" style={{ color: on ? '#fff' : theme.text }}>
                  {name}
                </ThemedText>
                <ThemedText type="small" style={{ color: on ? '#fff' : theme.textSecondary }}>
                  {lastPad[i] ?? '-'}
                </ThemedText>
              </View>
            );
          })}
        </Wrap>
      </Card>

      <Card title="Distance (sonar)">
        <ThemedText type="subtitle" style={{ color: distColor }}>
          {dist < 0 ? 'no echo' : `${dist} cm`}
        </ThemedText>
        <View style={[styles.barBg, { backgroundColor: theme.backgroundSelected }]}>
          <View style={[styles.barFill, { width: `${distPct}%`, backgroundColor: distColor }]} />
        </View>
        <ThemedText type="small" themeColor="textSecondary">
          Red below 10 cm (walking stops), orange below 30 cm (greeting).
        </ThemedText>
      </Card>

      {state?.imu ? (
        <Card title="IMU">
          {state.imu.ok ? (
            <Wrap gap={Spacing.three}>
              <Stat label="X" value={`${state.imu.x.toFixed(2)} g`} />
              <Stat label="Y" value={`${state.imu.y.toFixed(2)} g`} />
              <Stat label="Z" value={`${state.imu.z.toFixed(2)} g`} />
              <Stat label="Tilt" value={`${state.imu.tilt.toFixed(0)}°`} />
            </Wrap>
          ) : (
            <ThemedText style={{ color: '#FF3B30' }}>IMU not found</ThemedText>
          )}
        </Card>
      ) : null}

      <Card title="Hardware status">
        <Wrap gap={Spacing.three}>
          <Stat label="Audio" value={state?.audio ? 'ok' : 'FAIL'} />
          <Stat label="Pads" value={state?.padsOnline ? 'ok' : 'FAIL'} />
          {state?.pca !== undefined ? <Stat label="Servo driver" value={state.pca ? 'ok' : 'FAIL'} /> : null}
          {state?.cam ? <Stat label="Camera link" value={state.cam.alive ? `ok (${state.cam.state})` : 'offline'} /> : null}
          {state?.batt !== undefined ? <Stat label="Battery" value={state.batt < 0 ? 'none' : `${state.batt}%`} /> : null}
          <Stat label="Power" value={['active', 'dim', 'sleep'][state?.power ?? 0]} />
        </Wrap>
      </Card>

      <Card title="Display test">
        <Wrap>
          {DISPLAY_PATTERNS.map((p) => (
            <Chip key={p.id} label={p.label} onPress={() => send((c) => c.display(p.id))} />
          ))}
        </Wrap>
        <ThemedText type="small" themeColor="textSecondary">
          Look for dead pixels, colour errors and banding in the grey ramp. Choose Off to return to the face.
        </ThemedText>
      </Card>

      <Card title="Sound test">
        <Wrap>
          {[262, 440, 880, 1760].map((hz) => (
            <Chip key={hz} label={`${hz} Hz`} onPress={() => send((c) => c.beep(hz, 400))} />
          ))}
        </Wrap>
        <Wrap>
          {MELODIES.map((m, i) => (
            <Chip key={m} label={m} onPress={() => send((c) => c.melody(i))} />
          ))}
        </Wrap>
        <Button label="Stop sound" color="#FF3B30" onPress={() => send((c) => c.stopAudio())} />
      </Card>

        </>
      ) : null}
      {tab === 'simulate' ? (
        <>
      <Card title="Simulate a touch (no hands needed)">
        <Wrap>
          {padNames.map((n, i) => (
            <Chip key={n} label={n} active={simPad === i} onPress={() => setSimPad(i)} />
          ))}
        </Wrap>
        <Wrap>
          {[1, 2, 3, 4].map((g) => (
            <Chip key={g} label={GESTURES[g]} onPress={() => send((c) => c.simPad(simPad, g))} />
          ))}
          <Chip label="release" onPress={() => send((c) => c.simPad(simPad, 5))} />
        </Wrap>
        <ThemedText type="small" themeColor="textSecondary">
          Runs the same code path as a real touch. In Manual mode the robot only logs it.
        </ThemedText>
      </Card>

      <Card title="Simulate other events">
        <Wrap>
          {(isChiku ? (['shake', 'pickup', 'putdown', 'fall', 'tap', 'obstacle', 'touch', 'alarm'] as const) : (['obstacle', 'button'] as const)).map((ev) => (
            <Chip key={ev} label={ev} onPress={() => send((c) => c.simEvent(ev))} />
          ))}
        </Wrap>
      </Card>

        </>
      ) : null}
    </Page>
  );
}

const styles = StyleSheet.create({
  pad: { minWidth: 96, paddingVertical: 14, paddingHorizontal: 12, borderRadius: 14, alignItems: 'center', gap: 2 },
  barBg: { height: 14, borderRadius: 7, overflow: 'hidden' },
  barFill: { height: 14, borderRadius: 7 },
  logRow: { flexDirection: 'row', gap: Spacing.two },
  logTime: { width: 56, fontVariant: ['tabular-nums'] },
});

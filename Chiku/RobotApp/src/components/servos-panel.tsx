import Slider from '@react-native-community/slider';
import { useState } from 'react';
import { StyleSheet, View } from 'react-native';

import { SERVO_NAMES } from '@/api/catalog';
import { ThemedText } from '@/components/themed-text';
import { Button, Card, Chip, SwitchRow, Wrap, useAccent } from '@/components/ui';
import type { RobotState } from '@/api/types';
import { Spacing } from '@/constants/theme';
import { makeThrottle } from '@/lib/throttle';
import { useRobotState, useSend } from '@/hooks/use-robot';

interface Cfg {
  trim: number;
  min: number;
  max: number;
  inv: boolean;
}

function Stepper({ label, value, onChange, step = 1, lo, hi }: { label: string; value: number; onChange: (v: number) => void; step?: number; lo: number; hi: number }) {
  return (
    <View style={styles.stepper}>
      <ThemedText type="small" themeColor="textSecondary" style={styles.stepLabel}>
        {label}
      </ThemedText>
      <Chip label="-" onPress={() => onChange(Math.max(lo, value - step))} />
      <ThemedText type="smallBold" style={styles.stepValue}>
        {value}
      </ThemedText>
      <Chip label="+" onPress={() => onChange(Math.min(hi, value + step))} />
    </View>
  );
}

/** Servo sliders with trim / limits / reverse. Created once per robot so polling never overwrites edits. */
export function ServosPanel() {
  const { state, robot } = useRobotState();
  return state && robot ? <ServosBody key={robot.id} init={state} /> : null;
}

function ServosBody({ init }: { init: RobotState }) {
  const accent = useAccent();
  const send = useSend();
  const names = SERVO_NAMES[init.robot];
  // edited values start from the robot's current settings (once per robot)
  const [cfg, setCfg] = useState<Cfg[]>(() => init.servo.map((s) => ({ trim: s.trim, min: s.min, max: s.max, inv: s.inv })));
  const [angle, setAngle] = useState<number[]>(() => init.servo.map(() => 90));
  const [allow] = useState(() => makeThrottle(120));

  const setOne = (i: number, patch: Partial<Cfg>) => setCfg((c) => c.map((x, k) => (k === i ? { ...x, ...patch } : x)));

  const move = (i: number, deg: number, final: boolean) => {
    setAngle((a) => a.map((x, k) => (k === i ? deg : x)));
    if (allow(final)) send((c) => c.servoSet(i, deg, 120));
  };

  return (
    <>
      <Card>
        <ThemedText type="small" themeColor="textSecondary">
          Move each servo by hand to check direction and range. Fit the horns only after centring at 90° (see CALIBRATION.md). Servos release after 2 s without a move.
        </ThemedText>
        <Wrap>
          <Button label="Centre all (90°)" onPress={() => names.forEach((_, i) => { setAngle((a) => a.map(() => 90)); send((c) => c.servoSet(i, 90, 60)); })} />
          <Button label="Release all" color="#8E8E93" onPress={() => send((c) => c.servoRelease())} />
        </Wrap>
      </Card>

      {names.map((name, i) => (
        <Card key={name} title={`${i} · ${name}`}>
          <ThemedText type="subtitle">{Math.round(angle[i] ?? 90)}°</ThemedText>
          <Slider
            minimumValue={0}
            maximumValue={180}
            step={1}
            value={angle[i] ?? 90}
            minimumTrackTintColor={accent}
            onValueChange={(v) => move(i, v, false)}
            onSlidingComplete={(v) => move(i, v, true)}
          />
          <Wrap>
            {[45, 60, 90, 120, 135].map((d) => (
              <Chip key={d} label={`${d}°`} onPress={() => move(i, d, true)} />
            ))}
          </Wrap>
          {cfg[i] ? (
            <>
              <Stepper label="Trim" value={cfg[i].trim} lo={-45} hi={45} onChange={(v) => setOne(i, { trim: v })} />
              <Stepper label="Min" value={cfg[i].min} lo={0} hi={90} step={5} onChange={(v) => setOne(i, { min: v })} />
              <Stepper label="Max" value={cfg[i].max} lo={90} hi={180} step={5} onChange={(v) => setOne(i, { max: v })} />
              <SwitchRow label="Reverse direction" value={cfg[i].inv} onChange={(v) => setOne(i, { inv: v })} />
            </>
          ) : null}
        </Card>
      ))}

      <Button
        label="Save trims and limits"
        onPress={async () => {
          if (await send((c) => c.saveSettings({ servo: cfg }))) names.forEach((_, i) => send((c) => c.servoSet(i, angle[i] ?? 90, 120)));
        }}
      />
    </>
  );
}

const styles = StyleSheet.create({
  stepper: { flexDirection: 'row', alignItems: 'center', gap: Spacing.two },
  stepLabel: { width: 48 },
  stepValue: { minWidth: 36, textAlign: 'center' },
});

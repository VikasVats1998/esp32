// Small visual pieces for the Hub screen: ring-style value, bubble level, sparkline, level bar.
import { StyleSheet, View } from 'react-native';

import { ThemedText } from '@/components/themed-text';
import { useTheme } from '@/hooks/use-theme';

export const HUB = { cyan: '#00E5FF', orange: '#FFB35C', green: '#2BE08A', red: '#FF5C5C' } as const;

/** Horizontal bar, value 0..100. */
export function LevelBar({ value, color = HUB.green }: { value: number; color?: string }) {
  const theme = useTheme();
  return (
    <View style={[styles.bar, { backgroundColor: theme.backgroundSelected }]}>
      <View style={{ width: `${Math.max(0, Math.min(100, value))}%`, height: '100%', backgroundColor: color }} />
    </View>
  );
}

/** Bar chart of the last samples (no chart library needed). */
export function Sparkline({ values, color = HUB.cyan, min, max, height = 56 }: { values: number[]; color?: string; min?: number; max?: number; height?: number }) {
  const theme = useTheme();
  const shown = values.slice(-60);
  if (shown.length < 2) return <View style={{ height, borderBottomWidth: 1, borderColor: theme.backgroundSelected }} />;
  const lo = min ?? Math.min(...shown);
  let hi = max ?? Math.max(...shown);
  if (hi - lo < 1) hi = lo + 1;
  return (
    <View style={[styles.spark, { height, borderColor: theme.backgroundSelected }]}>
      {shown.map((v, i) => (
        <View key={i} style={{ flex: 1, height: Math.max(2, ((v - lo) / (hi - lo)) * height), backgroundColor: color, borderRadius: 2, opacity: 0.35 + 0.65 * (i / shown.length) }} />
      ))}
    </View>
  );
}

/** Bubble level: dot moves with roll / pitch (degrees, +-45 reaches the edge). */
export function BubbleLevel({ roll, pitch, size = 170 }: { roll: number; pitch: number; size?: number }) {
  const theme = useTheme();
  const R = size / 2 - 14;
  let x = Math.max(-1, Math.min(1, roll / 45)) * R;
  let y = Math.max(-1, Math.min(1, pitch / 45)) * R;
  const len = Math.hypot(x, y);
  if (len > R) {
    x *= R / len;
    y *= R / len;
  }
  const level = len < 6;
  return (
    <View style={{ width: size, height: size, alignSelf: 'center' }}>
      <View style={[styles.ring, { width: size, height: size, borderRadius: size / 2, borderColor: theme.backgroundSelected }]} />
      <View style={[styles.ring, { width: size / 2, height: size / 2, borderRadius: size / 4, left: size / 4, top: size / 4, borderColor: theme.backgroundSelected }]} />
      <View style={[styles.cross, { width: size, height: 1, top: size / 2, backgroundColor: theme.backgroundSelected }]} />
      <View style={[styles.cross, { width: 1, height: size, left: size / 2, backgroundColor: theme.backgroundSelected }]} />
      <View
        style={{
          position: 'absolute',
          width: 26,
          height: 26,
          borderRadius: 13,
          left: size / 2 + x - 13,
          top: size / 2 - y - 13,
          backgroundColor: level ? HUB.green : HUB.orange,
        }}
      />
    </View>
  );
}

export function BigNumber({ value, unit, color }: { value: string; unit?: string; color?: string }) {
  return (
    <View style={styles.big}>
      <ThemedText style={[styles.bigText, color ? { color } : null]}>{value}</ThemedText>
      {unit ? (
        <ThemedText type="small" themeColor="textSecondary">
          {unit}
        </ThemedText>
      ) : null}
    </View>
  );
}

const styles = StyleSheet.create({
  bar: { height: 10, borderRadius: 6, overflow: 'hidden' },
  spark: { flexDirection: 'row', alignItems: 'flex-end', gap: 2, borderBottomWidth: 1 },
  ring: { position: 'absolute', borderWidth: 2, left: 0, top: 0 },
  cross: { position: 'absolute' },
  big: { flexDirection: 'row', alignItems: 'baseline', gap: 6 },
  bigText: { fontSize: 34, fontWeight: '700', lineHeight: 40 },
});

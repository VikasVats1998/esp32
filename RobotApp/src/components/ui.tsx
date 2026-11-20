// Small set of building blocks used by all screens.
import { type ReactNode } from 'react';
import { Pressable, ScrollView, StyleSheet, Switch, Text, View, type ViewStyle } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';

import { ACCENT } from '@/api/catalog';
import { ThemedText } from '@/components/themed-text';
import { BottomTabInset, MaxContentWidth, Spacing } from '@/constants/theme';
import { useRobotState } from '@/hooks/use-robot';
import { useTheme } from '@/hooks/use-theme';
import { useUi } from '@/store/robots';

export function useAccent() {
  const { robot } = useRobotState();
  return ACCENT[robot?.kind ?? 'chiku'];
}

/** Page with a scroll area, title, connection banner and error banner. */
export function Page({ title, children }: { title: string; children: ReactNode }) {
  const theme = useTheme();
  const { robot, connected, error } = useRobotState();
  const lastError = useUi((s) => s.error);
  const setError = useUi((s) => s.setError);
  const accent = useAccent();
  return (
    <View style={[styles.flex, { backgroundColor: theme.background }]}>
      <SafeAreaView style={styles.flex} edges={['top']}>
        <ScrollView
          contentContainerStyle={[styles.page, { paddingBottom: BottomTabInset + Spacing.six }]}
          keyboardShouldPersistTaps="handled">
          <View style={styles.titleRow}>
            <ThemedText type="subtitle">{title}</ThemedText>
            {robot ? (
              <View style={[styles.pill, { backgroundColor: theme.backgroundElement }]}>
                <View style={[styles.dot, { backgroundColor: connected ? '#34C759' : error ? '#FF3B30' : '#FF9F0A' }]} />
                <ThemedText type="smallBold" style={{ color: accent }}>
                  {robot.name}
                </ThemedText>
              </View>
            ) : null}
          </View>
          {lastError ? (
            <Pressable onPress={() => setError(undefined)} style={[styles.banner, { backgroundColor: '#FF3B3022' }]}>
              <ThemedText type="small" style={{ color: '#FF3B30' }}>
                {lastError} (tap to hide)
              </ThemedText>
            </Pressable>
          ) : null}
          {robot && !connected ? (
            <View style={[styles.banner, { backgroundColor: '#FF9F0A22' }]}>
              <ThemedText type="small" style={{ color: '#FF9F0A' }}>
                {error ? `Cannot reach ${robot.host}: ${error.message}` : 'Connecting...'}
              </ThemedText>
            </View>
          ) : null}
          {!robot ? (
            <View style={[styles.banner, { backgroundColor: theme.backgroundElement }]}>
              <ThemedText type="small">No robot selected. Go to the Robots tab and add Chiku or Jinu.</ThemedText>
            </View>
          ) : null}
          {children}
        </ScrollView>
      </SafeAreaView>
    </View>
  );
}

export function Card({ title, children, style }: { title?: string; children: ReactNode; style?: ViewStyle }) {
  const theme = useTheme();
  return (
    <View style={[styles.card, { backgroundColor: theme.backgroundElement }, style]}>
      {title ? (
        <ThemedText type="smallBold" themeColor="textSecondary" style={styles.cardTitle}>
          {title.toUpperCase()}
        </ThemedText>
      ) : null}
      {children}
    </View>
  );
}

export function Wrap({ children, gap = Spacing.two }: { children: ReactNode; gap?: number }) {
  return <View style={[styles.wrap, { gap }]}>{children}</View>;
}

export function Chip({
  label,
  onPress,
  active,
  color,
  disabled,
}: {
  label: string;
  onPress?: () => void;
  active?: boolean;
  color?: string;
  disabled?: boolean;
}) {
  const theme = useTheme();
  const accent = useAccent();
  const c = color ?? accent;
  return (
    <Pressable
      disabled={disabled}
      onPress={onPress}
      style={({ pressed }) => [
        styles.chip,
        { backgroundColor: active ? c : theme.backgroundSelected, opacity: disabled ? 0.4 : pressed ? 0.6 : 1 },
      ]}>
      <Text style={{ color: active ? '#fff' : theme.text, fontWeight: '600' }}>{label}</Text>
    </Pressable>
  );
}

export function Button({ label, onPress, color, disabled }: { label: string; onPress: () => void; color?: string; disabled?: boolean }) {
  const accent = useAccent();
  return (
    <Pressable
      disabled={disabled}
      onPress={onPress}
      style={({ pressed }) => [styles.button, { backgroundColor: color ?? accent, opacity: disabled ? 0.4 : pressed ? 0.7 : 1 }]}>
      <Text style={styles.buttonText}>{label}</Text>
    </Pressable>
  );
}

export function Segmented<T extends string>({
  options,
  value,
  onChange,
}: {
  options: { value: T; label: string }[];
  value: T;
  onChange: (v: T) => void;
}) {
  const theme = useTheme();
  const accent = useAccent();
  return (
    <View style={[styles.segment, { backgroundColor: theme.backgroundSelected }]}>
      {options.map((o) => (
        <Pressable key={o.value} onPress={() => onChange(o.value)} style={[styles.segmentItem, o.value === value && { backgroundColor: accent }]}>
          <Text style={{ color: o.value === value ? '#fff' : theme.text, fontWeight: '700' }}>{o.label}</Text>
        </Pressable>
      ))}
    </View>
  );
}

export function SwitchRow({ label, value, onChange, hint }: { label: string; value: boolean; onChange: (v: boolean) => void; hint?: string }) {
  const accent = useAccent();
  return (
    <View style={styles.switchRow}>
      <View style={styles.flex}>
        <ThemedText>{label}</ThemedText>
        {hint ? (
          <ThemedText type="small" themeColor="textSecondary">
            {hint}
          </ThemedText>
        ) : null}
      </View>
      <Switch value={value} onValueChange={onChange} trackColor={{ true: accent }} />
    </View>
  );
}

export function Stat({ label, value }: { label: string; value: string }) {
  return (
    <View style={styles.stat}>
      <ThemedText type="small" themeColor="textSecondary">
        {label}
      </ThemedText>
      <ThemedText type="smallBold">{value}</ThemedText>
    </View>
  );
}

const styles = StyleSheet.create({
  flex: { flex: 1 },
  page: { padding: Spacing.three, gap: Spacing.three, maxWidth: MaxContentWidth, width: '100%', alignSelf: 'center' },
  titleRow: { flexDirection: 'row', alignItems: 'center', justifyContent: 'space-between' },
  pill: { flexDirection: 'row', alignItems: 'center', gap: 6, paddingHorizontal: 12, paddingVertical: 6, borderRadius: 999 },
  dot: { width: 9, height: 9, borderRadius: 5 },
  banner: { padding: Spacing.three, borderRadius: 12 },
  card: { borderRadius: 16, padding: Spacing.three, gap: Spacing.two },
  cardTitle: { letterSpacing: 0.6 },
  wrap: { flexDirection: 'row', flexWrap: 'wrap' },
  chip: { paddingHorizontal: 14, paddingVertical: 10, borderRadius: 12 },
  button: { paddingHorizontal: 16, paddingVertical: 12, borderRadius: 12, alignItems: 'center' },
  buttonText: { color: '#fff', fontWeight: '700' },
  segment: { flexDirection: 'row', borderRadius: 12, padding: 3 },
  segmentItem: { flex: 1, paddingVertical: 10, borderRadius: 10, alignItems: 'center' },
  switchRow: { flexDirection: 'row', alignItems: 'center', gap: Spacing.three },
  stat: { minWidth: 96, gap: 2 },
});

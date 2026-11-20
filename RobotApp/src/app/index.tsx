import { useState } from 'react';
import { Alert, StyleSheet, TextInput, View } from 'react-native';

import { DEFAULT_HOST } from '@/api/catalog';
import { normaliseHost } from '@/api/client';
import type { RobotKind } from '@/api/types';
import { ThemedText } from '@/components/themed-text';
import { Button, Card, Chip, Page, Segmented, Stat, Wrap } from '@/components/ui';
import { Spacing } from '@/constants/theme';
import { useRobotState } from '@/hooks/use-robot';
import { useTheme } from '@/hooks/use-theme';
import { scanNetwork } from '@/lib/scan';
import { useRobots } from '@/store/robots';

function fmtUptime(s: number) {
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  return h ? `${h} h ${m} min` : `${m} min ${s % 60} s`;
}

export default function RobotsScreen() {
  const theme = useTheme();
  const robots = useRobots((s) => s.robots);
  const activeId = useRobots((s) => s.activeId);
  const add = useRobots((s) => s.add);
  const remove = useRobots((s) => s.remove);
  const setActive = useRobots((s) => s.setActive);
  const { state, connected } = useRobotState();

  const [kind, setKind] = useState<RobotKind>('chiku');
  const [host, setHost] = useState('');
  const [scanning, setScanning] = useState(false);
  const [progress, setProgress] = useState(0);
  const [scanMsg, setScanMsg] = useState('');

  const addManual = () => {
    const h = normaliseHost(host || DEFAULT_HOST[kind]);
    add({ name: kind === 'chiku' ? 'Chiku' : 'Jinu', host: h, kind });
    setHost('');
  };

  const scan = async () => {
    setScanning(true);
    setScanMsg('');
    setProgress(0);
    try {
      const found = await scanNetwork((d, t) => setProgress(Math.round((d / t) * 100)));
      found.forEach((f) => add({ name: f.state.robot === 'chiku' ? 'Chiku' : 'Jinu', host: f.host, kind: f.state.robot }));
      setScanMsg(found.length ? `Found ${found.map((f) => `${f.state.robot} (${f.host})`).join(', ')}` : 'No robot found. Same Wi-Fi? Is it powered?');
    } catch (e) {
      setScanMsg((e as Error).message);
    } finally {
      setScanning(false);
    }
  };

  return (
    <Page title="Robots">
      {state ? (
        <Card title="Connected robot">
          <ThemedText type="smallBold">
            {state.robot.toUpperCase()} · firmware {state.fw} · API {state.api}
          </ThemedText>
          <Wrap gap={Spacing.three}>
            <Stat label="Address" value={state.ip || '-'} />
            <Stat label="Wi-Fi" value={`${state.ssid || '-'} (${state.rssi} dBm)`} />
            <Stat label="Uptime" value={fmtUptime(state.uptime)} />
            <Stat label="Mode" value={state.mode} />
            <Stat label="Mood" value={state.mood} />
            <Stat label="Free heap" value={`${Math.round(state.heap / 1024)} KB`} />
            {state.batt !== undefined ? <Stat label="Battery" value={state.batt < 0 ? 'none' : `${state.batt}%${state.charging ? ' charging' : ''}`} /> : null}
          </Wrap>
          <ThemedText type="small" themeColor="textSecondary">
            Features: {state.caps.join(', ') || 'none'}
          </ThemedText>
        </Card>
      ) : null}

      <Card title="My robots">
        {robots.length === 0 ? <ThemedText themeColor="textSecondary">None yet. Scan the network or add one below.</ThemedText> : null}
        {robots.map((r) => (
          <View key={r.id} style={styles.robotRow}>
            <View style={styles.flex}>
              <ThemedText type="smallBold">{r.name}</ThemedText>
              <ThemedText type="small" themeColor="textSecondary">
                {r.host}
              </ThemedText>
            </View>
            <Chip label={r.id === activeId ? (connected ? 'Connected' : 'Selected') : 'Use'} active={r.id === activeId} onPress={() => setActive(r.id)} />
            <Chip
              label="Remove"
              color="#FF3B30"
              onPress={() => Alert.alert('Remove robot', `Remove ${r.name} (${r.host})?`, [{ text: 'Cancel' }, { text: 'Remove', style: 'destructive', onPress: () => remove(r.id) }])}
            />
          </View>
        ))}
      </Card>

      <Card title="Find robots">
        <Button label={scanning ? `Scanning ${progress}%` : 'Scan Wi-Fi network'} onPress={scan} disabled={scanning} />
        {scanMsg ? <ThemedText type="small">{scanMsg}</ThemedText> : null}
        <ThemedText type="small" themeColor="textSecondary">
          The phone must be on the same Wi-Fi as the robot. The robot shows its address on its Settings screen (Chiku) or on the web page.
        </ThemedText>
      </Card>

      <Card title="Add by address">
        <Segmented
          options={[
            { value: 'chiku' as RobotKind, label: 'Chiku' },
            { value: 'jinu' as RobotKind, label: 'Jinu' },
          ]}
          value={kind}
          onChange={setKind}
        />
        <TextInput
          value={host}
          onChangeText={setHost}
          placeholder={`${DEFAULT_HOST[kind]} or 192.168.1.50`}
          placeholderTextColor={theme.textSecondary}
          autoCapitalize="none"
          autoCorrect={false}
          style={[styles.input, { color: theme.text, backgroundColor: theme.backgroundSelected }]}
        />
        <Button label="Add robot" onPress={addManual} />
        {__DEV__ ? (
          <Wrap>
            <Chip label="Mock Chiku :8787" onPress={() => add({ name: 'Mock Chiku', host: 'localhost:8787', kind: 'chiku' })} />
            <Chip label="Mock Jinu :8788" onPress={() => add({ name: 'Mock Jinu', host: 'localhost:8788', kind: 'jinu' })} />
            <Chip label="Android emulator Chiku" onPress={() => add({ name: 'Mock Chiku', host: '10.0.2.2:8787', kind: 'chiku' })} />
          </Wrap>
        ) : null}
      </Card>
    </Page>
  );
}

const styles = StyleSheet.create({
  flex: { flex: 1 },
  robotRow: { flexDirection: 'row', alignItems: 'center', gap: Spacing.two },
  input: { borderRadius: 12, paddingHorizontal: 14, paddingVertical: 12, fontSize: 16 },
});

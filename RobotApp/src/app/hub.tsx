// Hub tab: live sensors from the robot, the devices on the hub server, voice / sensor settings.
import Slider from '@react-native-community/slider';
import { useEffect, useState } from 'react';
import { ScrollView, StyleSheet, Switch, TextInput, View } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';

import { BigNumber, BubbleLevel, HUB, LevelBar, Sparkline } from '@/components/hub-widgets';
import { ThemedText } from '@/components/themed-text';
import { Button, Card, Stat } from '@/components/ui';
import { BottomTabInset, MaxContentWidth, Spacing } from '@/constants/theme';
import { useHubActions, useHubHistory, useHubState } from '@/hooks/use-hub';
import { useTheme } from '@/hooks/use-theme';
import type { HubDevice } from '@/hub/client';
import { useHub } from '@/store/hub';

function ConnectionCard({ online, error }: { online: boolean; error: Error | null }) {
  const theme = useTheme();
  const url = useHub((s) => s.url);
  const token = useHub((s) => s.token);
  const save = useHub((s) => s.save);
  const [editU, setU] = useState<string | undefined>();   // undefined = show the saved value
  const [editT, setT] = useState<string | undefined>();
  const u = editU ?? url;
  const t = editT ?? token;
  const input = [styles.input, { color: theme.text, backgroundColor: theme.backgroundSelected }];
  return (
    <Card title="Hub server">
      <View style={styles.row}>
        <View style={[styles.dot, { backgroundColor: online ? HUB.green : url ? HUB.red : theme.textSecondary }]} />
        <ThemedText type="smallBold">{online ? 'Connected' : url ? 'Offline' : 'Not set'}</ThemedText>
      </View>
      <TextInput value={u} onChangeText={setU} placeholder="192.168.1.20:8080" placeholderTextColor={theme.textSecondary} autoCapitalize="none" autoCorrect={false} keyboardType="url" style={input} />
      <TextInput value={t} onChangeText={setT} placeholder="token" placeholderTextColor={theme.textSecondary} autoCapitalize="none" autoCorrect={false} style={input} />
      <Button
        label="Save"
        onPress={() => {
          save(u, t);
          setU(undefined);
          setT(undefined);
        }}
        color={HUB.cyan}
      />
      {error && url ? (
        <ThemedText type="small" style={{ color: HUB.red }}>
          {error.message}. Run `python3 Server/server.py` on the PC and use its IP address.
        </ThemedText>
      ) : null}
    </Card>
  );
}

function DeviceCard({ d }: { d: HubDevice }) {
  const { device } = useHubActions();
  const [drag, setDrag] = useState<number | null>(null);   // value while the finger is on the slider
  const level = drag ?? d.level;
  const color = d.type === 'light' ? HUB.orange : d.type === 'fan' ? HUB.cyan : HUB.green;
  const unit = d.type === 'climate' ? '°C' : '%';
  return (
    <Card>
      <View style={styles.row}>
        <View style={{ flex: 1 }}>
          <ThemedText type="smallBold">{d.name}</ThemedText>
          <ThemedText type="small" themeColor="textSecondary">
            {d.load ? `${d.load} W` : d.type}
            {d.type === 'light' && d.temp ? ` · ${d.temp} K` : ''}
          </ThemedText>
        </View>
        <Switch value={d.on} onValueChange={(on) => device.mutate({ id: d.id, on })} trackColor={{ true: color }} />
      </View>
      <BigNumber value={String(d.on ? level : 0)} unit={unit} color={d.on ? color : undefined} />
      <Slider
        value={level}
        minimumValue={0}
        maximumValue={d.type === 'climate' ? 30 : 100}
        step={1}
        minimumTrackTintColor={color}
        onValueChange={(v) => setDrag(Math.round(v))}
        onSlidingComplete={(v) => {
          device.mutate({ id: d.id, level: Math.round(v) });
          setDrag(null);
        }}
      />
    </Card>
  );
}

export default function HubScreen() {
  const theme = useTheme();
  const load = useHub((s) => s.load);
  const { state, robotId, online, error } = useHubState();
  const history = useHubHistory(robotId);
  const { robot } = useHubActions();
  useEffect(() => {
    load();
  }, [load]);

  const r = robotId ? state?.robots[robotId] : undefined;
  const h = history.data ?? [];
  const series = (k: 'snd' | 'bat' | 'temp') => h.map((s) => s[k]).filter((v): v is number => typeof v === 'number');
  const set = (cfg: Parameters<typeof robot.mutate>[0]['cfg']) => {
    if (robotId) robot.mutate({ robot: robotId, cfg });
  };

  return (
    <View style={[styles.flex, { backgroundColor: theme.background }]}>
      <SafeAreaView style={styles.flex} edges={['top']}>
        <ScrollView contentContainerStyle={[styles.page, { paddingBottom: BottomTabInset + Spacing.six }]} keyboardShouldPersistTaps="handled">
          <ThemedText type="subtitle">Hub</ThemedText>
          <ConnectionCard online={online} error={error} />

          {r ? (
            <>
              <Card title={`Motion · ${robotId}${r.age > 5 ? ' (offline)' : ''}`}>
                <BubbleLevel roll={r.roll ?? 0} pitch={r.pitch ?? 0} />
                <View style={styles.stats}>
                  <Stat label="Tilt" value={`${r.tilt ?? '-'}°`} />
                  <Stat label="Rotation" value={`${r.gyro ?? '-'} °/s`} />
                  <Stat label="Roll / pitch" value={`${r.roll ?? '-'}° / ${r.pitch ?? '-'}°`} />
                  <Stat label="Orientation" value={r.flip ? 'upside down' : 'upright'} />
                </View>
                <Button label="Recalibrate (keep still)" onPress={() => set({ calibrate: true })} color={HUB.cyan} />
              </Card>

              <Card title="Sound · wake">
                <BigNumber value={String(r.snd ?? '-')} unit="% loudness" color={HUB.cyan} />
                <LevelBar value={r.snd ?? 0} color={HUB.cyan} />
                <Sparkline values={series('snd')} min={0} max={100} />
                <View style={styles.row}>
                  <ThemedText style={{ flex: 1 }}>Clap wake</ThemedText>
                  <Switch value={!!r.clapWake} onValueChange={(v) => set({ clapWake: v })} trackColor={{ true: HUB.green }} />
                </View>
                <ThemedText type="small" themeColor="textSecondary">
                  Clap sensitivity
                </ThemedText>
                <Slider value={r.clapSens ?? 60} minimumValue={0} maximumValue={100} step={5} minimumTrackTintColor={HUB.cyan} onSlidingComplete={(v) => set({ clapSens: Math.round(v) })} />
                <View style={styles.row}>
                  <View style={{ flex: 1 }}>
                    <ThemedText>Voice wake word</ThemedText>
                    <ThemedText type="small" themeColor="textSecondary">
                      Say &quot;Hi ESP&quot;, then a command. Applies after a restart.
                    </ThemedText>
                  </View>
                  <Switch value={!!r.wakeWord} onValueChange={(v) => set({ wakeWord: v })} trackColor={{ true: HUB.green }} />
                </View>
              </Card>

              <Card title="Power · temperature">
                <View style={styles.row}>
                  <BigNumber value={r.bat != null && r.bat >= 0 ? `${r.bat}` : '--'} unit="%" color={HUB.green} />
                  <ThemedText type="small" themeColor="textSecondary" style={{ flex: 1, textAlign: 'right' }}>
                    {r.batv ?? '-'} V{r.chg ? ' · charging' : ''}
                    {r.usb ? ' · USB' : ''}
                  </ThemedText>
                </View>
                <LevelBar value={r.bat ?? 0} />
                <Sparkline values={series('bat')} color={HUB.green} min={0} max={100} />
                <View style={styles.stats}>
                  <Stat label="Core temperature" value={`${r.temp ?? '-'} °C`} />
                  <Stat label="Wi-Fi" value={`${r.rssi ?? '-'} dBm`} />
                  <Stat label="Free memory" value={`${r.heap ?? '-'} KB`} />
                  <Stat label="Firmware" value={r.fw ?? '-'} />
                </View>
                <Sparkline values={series('temp')} color={HUB.orange} />
              </Card>
            </>
          ) : online ? (
            <Card>
              <ThemedText type="small">No robot has connected to the hub yet. On the robot: POST /api/set with {'{"serverUrl": "http://<pc-ip>:8080"}'}.</ThemedText>
            </Card>
          ) : null}

          {state?.devices.map((d) => <DeviceCard key={d.id} d={d} />)}
        </ScrollView>
      </SafeAreaView>
    </View>
  );
}

const styles = StyleSheet.create({
  flex: { flex: 1 },
  page: { padding: Spacing.three, gap: Spacing.three, maxWidth: MaxContentWidth, width: '100%', alignSelf: 'center' },
  row: { flexDirection: 'row', alignItems: 'center', gap: Spacing.two },
  dot: { width: 10, height: 10, borderRadius: 5 },
  stats: { flexDirection: 'row', flexWrap: 'wrap', columnGap: Spacing.four, rowGap: Spacing.two },
  input: { borderRadius: 10, paddingHorizontal: 12, paddingVertical: 10, fontSize: 16 },
});

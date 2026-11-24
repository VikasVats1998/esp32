import Slider from '@react-native-community/slider';
import * as DocumentPicker from 'expo-document-picker';
import { useState } from 'react';
import { Alert, Linking, StyleSheet, TextInput, View } from 'react-native';

import { ThemedText } from '@/components/themed-text';
import { Button, Card, Chip, Page, Stat, SwitchRow, Wrap, useAccent } from '@/components/ui';
import type { RobotState } from '@/api/types';
import { Spacing } from '@/constants/theme';
import { useRobotState, useSend } from '@/hooks/use-robot';
import { useTheme } from '@/hooks/use-theme';
import { useUi } from '@/store/robots';

const DAYS = ['Mo', 'Tu', 'We', 'Th', 'Fr', 'Sa', 'Su'];

function Labeled({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <View style={styles.labeled}>
      <ThemedText type="smallBold">{label}</ThemedText>
      {children}
    </View>
  );
}

export default function SettingsScreen() {
  const { state, robot } = useRobotState();
  // The body is created once per robot with the robot's current values, so polling never overwrites edits.
  return <Page title="Settings">{state && robot ? <SettingsBody key={robot.id} init={state} /> : null}</Page>;
}

function SettingsBody({ init }: { init: RobotState }) {
  const theme = useTheme();
  const accent = useAccent();
  const send = useSend();
  const setError = useUi((s) => s.setError);
  const { state, client } = useRobotState();
  const isChiku = init.robot === 'chiku';

  const [bri, setBri] = useState(init.brightness);
  const [vol, setVol] = useState(init.volume);
  const [lat, setLat] = useState(init.lat !== undefined ? String(init.lat) : '');
  const [lon, setLon] = useState(init.lon !== undefined ? String(init.lon) : '');
  const [alarms, setAlarms] = useState(init.alarms ?? []);
  const [busy, setBusy] = useState('');

  const pick = async (types: string[]) => {
    const res = await DocumentPicker.getDocumentAsync({ type: types, copyToCacheDirectory: true });
    return res.canceled ? undefined : res.assets[0];
  };

  const uploadFirmware = async () => {
    const f = await pick(['application/octet-stream', '*/*']);
    if (!f || !client) return;
    if (!f.name.endsWith('.bin')) return setError('Choose the firmware .bin file (.pio/build/<env>/firmware.bin).');
    Alert.alert('Update firmware', `Upload ${f.name} (${Math.round((f.size ?? 0) / 1024)} KB)? The robot reboots afterwards.`, [
      { text: 'Cancel' },
      {
        text: 'Upload',
        onPress: async () => {
          setBusy('Uploading firmware...');
          try {
            await client.uploadFirmware({ uri: f.uri, name: f.name, type: 'application/octet-stream' });
            setError('Firmware uploaded. The robot is rebooting.');
          } catch (e) {
            setError((e as Error).message);
          } finally {
            setBusy('');
          }
        },
      },
    ]);
  };

  const uploadVoice = async () => {
    const f = await pick(['audio/wav', 'audio/x-wav', '*/*']);
    if (!f || !client) return;
    if (!f.name.endsWith('.wav')) return setError('Choose a .wav file (16 kHz, 16-bit, mono). The file name is the voice name, e.g. hello.wav.');
    setBusy('Uploading voice...');
    try {
      await client.uploadVoice({ uri: f.uri, name: f.name, type: 'audio/wav' });
      setError(undefined);
    } catch (e) {
      setError((e as Error).message);
    } finally {
      setBusy('');
    }
  };

  const confirm = (title: string, msg: string, action: () => void) => Alert.alert(title, msg, [{ text: 'Cancel' }, { text: 'Yes', style: 'destructive', onPress: action }]);

  const setAlarm = (i: number, patch: Partial<(typeof alarms)[number]>) => setAlarms((a) => a.map((x, k) => (k === i ? { ...x, ...patch } : x)));

  return (
    <>
      {busy ? (
        <Card>
          <ThemedText>{busy}</ThemedText>
        </Card>
      ) : null}

      <Card title="Display and sound">
        <Labeled label={`Brightness ${Math.round((bri * 100) / 255)}%`}>
          <Slider minimumValue={10} maximumValue={255} step={5} value={bri} minimumTrackTintColor={accent} onValueChange={setBri} onSlidingComplete={(v) => send((c) => c.saveSettings({ brightness: v }))} />
        </Labeled>
        <Labeled label={`Volume ${vol}%`}>
          <Slider minimumValue={0} maximumValue={100} step={5} value={vol} minimumTrackTintColor={accent} onValueChange={setVol} onSlidingComplete={(v) => send(async (c) => { await c.saveSettings({ volume: v }); await c.beep(880, 150); })} />
        </Labeled>
      </Card>

      {isChiku ? (
        <>
          <Card title="Weather location">
            <View style={styles.row}>
              <TextInput value={lat} onChangeText={setLat} placeholder="latitude" keyboardType="numbers-and-punctuation" placeholderTextColor={theme.textSecondary} style={[styles.input, { color: theme.text, backgroundColor: theme.backgroundSelected }]} />
              <TextInput value={lon} onChangeText={setLon} placeholder="longitude" keyboardType="numbers-and-punctuation" placeholderTextColor={theme.textSecondary} style={[styles.input, { color: theme.text, backgroundColor: theme.backgroundSelected }]} />
            </View>
            <Wrap>
              <Button label="Save location" onPress={() => send((c) => c.saveSettings({ lat: parseFloat(lat), lon: parseFloat(lon) }))} />
              <Button label="Refresh weather" color="#8E8E93" onPress={() => send((c) => c.refreshWeather())} />
            </Wrap>
            {state?.weather?.valid ? (
              <ThemedText type="small" themeColor="textSecondary">
                {state.weather.text}, {state.weather.temp} °C (min {state.weather.min} / max {state.weather.max})
              </ThemedText>
            ) : null}
          </Card>

          <Card title="Alarms">
            {alarms.map((a, i) => (
              <View key={i} style={styles.alarm}>
                <View style={styles.row}>
                  <SwitchRow label={`Alarm ${i + 1}`} value={a.on} onChange={(v) => setAlarm(i, { on: v })} />
                </View>
                <View style={styles.row}>
                  <Chip label="-h" onPress={() => setAlarm(i, { h: (a.h + 23) % 24 })} />
                  <ThemedText type="smallBold">{String(a.h).padStart(2, '0')}:{String(a.m).padStart(2, '0')}</ThemedText>
                  <Chip label="+h" onPress={() => setAlarm(i, { h: (a.h + 1) % 24 })} />
                  <Chip label="-5m" onPress={() => setAlarm(i, { m: (a.m + 55) % 60 })} />
                  <Chip label="+5m" onPress={() => setAlarm(i, { m: (a.m + 5) % 60 })} />
                </View>
                <Wrap gap={4}>
                  {DAYS.map((d, k) => (
                    <Chip key={d} label={d} active={(a.days & (1 << k)) !== 0} onPress={() => setAlarm(i, { days: a.days ^ (1 << k) })} />
                  ))}
                </Wrap>
              </View>
            ))}
            <Wrap>
              <Button label="Save alarms" onPress={() => send((c) => c.saveSettings({ alarms }))} />
              <Button label="Test ring" color="#8E8E93" onPress={() => send((c) => c.simEvent('alarm'))} />
            </Wrap>
          </Card>

          <Card title="Timers">
            <Wrap>
              {[60, 300, 600].map((s) => (
                <Chip key={s} label={`${s / 60} min`} onPress={() => send((c) => c.startTimer(s))} />
              ))}
              <Chip label="Cancel timer 1" color="#FF3B30" onPress={() => send((c) => c.cancelTimer(0))} />
            </Wrap>
            <ThemedText type="small" themeColor="textSecondary">
              Running: {(state?.timers ?? []).map((t, i) => (t ? `#${i + 1} ${t}s` : '')).filter(Boolean).join('  ') || 'none'}
            </ThemedText>
          </Card>

          <Card title="Camera (ESP32-CAM)">
            <Stat label="Status" value={state?.cam?.alive ? `online (${state.cam.state}) ${state.cam.ip}` : 'offline'} />
            <Wrap>
              <Chip label="Wake" onPress={() => send((c) => c.cam('wake'))} />
              <Chip label="Sleep" onPress={() => send((c) => c.cam('sleep'))} />
              <Chip label="Light on" onPress={() => send((c) => c.cam('led_on'))} />
              <Chip label="Light off" onPress={() => send((c) => c.cam('led_off'))} />
              <Chip label="Snapshot" onPress={() => send((c) => c.cam('snap'))} />
            </Wrap>
            <Button label="Open live stream" disabled={!state?.cam?.alive || !state.cam.ip} onPress={() => state?.cam && Linking.openURL(state.cam.stream)} />
          </Card>
        </>
      ) : null}

      <Card title="Voice clips">
        <ThemedText type="small" themeColor="textSecondary">
          Upload a 16 kHz, 16-bit, mono .wav. The file name is the voice name (hello.wav replaces the greeting).
        </ThemedText>
        <Button label="Upload a voice clip" onPress={uploadVoice} />
      </Card>

      <Card title="System">
        <Wrap>
          <Button label="Update firmware (.bin)" onPress={uploadFirmware} />
          <Button label="Reboot" color="#FF9F0A" onPress={() => confirm('Reboot', 'Restart the robot?', () => send((c) => c.reboot()))} />
          <Button label="Forget Wi-Fi" color="#FF3B30" onPress={() => confirm('Forget Wi-Fi', 'The robot will open its setup portal again.', () => send((c) => c.wifiReset()))} />
        </Wrap>
        <ThemedText type="small" themeColor="textSecondary">
          Firmware file: .pio/build/head/firmware.bin (Chiku) or .pio/build/jinu/firmware.bin (Jinu).
        </ThemedText>
      </Card>
    </>
  );
}

const styles = StyleSheet.create({
  labeled: { gap: 4 },
  row: { flexDirection: 'row', alignItems: 'center', gap: Spacing.two, flexWrap: 'wrap' },
  input: { flex: 1, minWidth: 120, borderRadius: 12, paddingHorizontal: 14, paddingVertical: 12, fontSize: 16 },
  alarm: { gap: Spacing.two, paddingBottom: Spacing.two },
});

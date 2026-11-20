// Desk control: pairing, height, hold-to-move buttons, presets, child lock, robots, diagnostics.
import { useEffect, useRef, useState } from 'react';
import { Alert, Pressable, StyleSheet, Text, TextInput, View } from 'react-native';

import { ThemedText } from '@/components/themed-text';
import { Button, Card, Chip, Stat, SwitchRow, Wrap } from '@/components/ui';
import { Spacing } from '@/constants/theme';
import type { DeskLine } from '@/desk/client';
import { useDeskSend, useDeskState } from '@/hooks/use-desk';
import { useRobotState, useSend } from '@/hooks/use-robot';
import { useTheme } from '@/hooks/use-theme';
import { normaliseHost } from '@/api/client';
import { useDesk } from '@/store/desk';
import { useUi } from '@/store/robots';

function HoldButton({ label, dir, onMove, onStop }: { label: string; dir: 'up' | 'down'; onMove: (d: 'up' | 'down') => void; onStop: () => void }) {
  const theme = useTheme();
  const timer = useRef<ReturnType<typeof setInterval> | undefined>(undefined);
  const [down, setDown] = useState(false);

  const stopTimer = () => {
    if (timer.current) clearInterval(timer.current);
    timer.current = undefined;
  };
  useEffect(() => stopTimer, []);

  return (
    <Pressable
      onPressIn={() => {
        setDown(true);
        onMove(dir);
        stopTimer();
        timer.current = setInterval(() => onMove(dir), 250); // dead-man: the desk stops by itself when this stops
      }}
      onPressOut={() => {
        setDown(false);
        stopTimer();
        onStop();
      }}
      style={[styles.hold, { backgroundColor: down ? '#34C759' : theme.backgroundSelected }]}>
      <Text style={[styles.holdText, { color: down ? '#fff' : theme.text }]}>{label}</Text>
    </Pressable>
  );
}

export function DeskPanel() {
  const theme = useTheme();
  const send = useDeskSend();
  const sendRobot = useSend();
  const setError = useUi((s) => s.setError);
  const host = useDesk((s) => s.host);
  const token = useDesk((s) => s.token);
  const pair = useDesk((s) => s.pair);
  const forget = useDesk((s) => s.forget);
  const { client, state, connected, error } = useDeskState();
  const { state: robotState, robot } = useRobotState();

  const [hostInput, setHostInput] = useState('desk.local');
  const [pairing, setPairing] = useState(false);
  const [calib, setCalib] = useState('');
  const [lines, setLines] = useState<DeskLine[]>([]);
  const [raw, setRaw] = useState('');

  const doPair = async () => {
    setPairing(true);
    try {
      await pair(normaliseHost(hostInput), 'phone app');
      setError(undefined);
    } catch (e) {
      setError((e as Error).message);
    } finally {
      setPairing(false);
    }
  };

  const confirm = (title: string, msg: string, action: () => void) => Alert.alert(title, msg, [{ text: 'Cancel' }, { text: 'Yes', onPress: action }]);

  // ---------------- not paired ----------------
  if (!client) {
    return (
      <>
        <Card title="Pair with the desk module">
          <ThemedText type="small" themeColor="textSecondary">
            1. Power-cycle the desk (unplug and plug in). The pairing window of the module is open for 3 minutes.{'\n'}
            2. Your phone must be on the same Wi-Fi as the module.{'\n'}
            3. Enter its address and press Pair. The key is worked out on both sides (never sent over the air) and kept in the secure storage of the phone.
          </ThemedText>
          <TextInput
            value={hostInput}
            onChangeText={setHostInput}
            autoCapitalize="none"
            autoCorrect={false}
            placeholder="desk.local or IP address"
            placeholderTextColor={theme.textSecondary}
            style={[styles.input, { color: theme.text, backgroundColor: theme.backgroundSelected }]}
          />
          <Button label={pairing ? 'Pairing...' : 'Pair'} disabled={pairing} onPress={doPair} />
        </Card>
      </>
    );
  }

  const h = state?.hv ? state.h.toFixed(1) : '--.-';

  return (
    <>
      <Card title={`Desk ${host}`}>
        <View style={styles.heightRow}>
          <ThemedText type="title" style={{ color: connected ? theme.text : theme.textSecondary }}>
            {h}
          </ThemedText>
          <ThemedText themeColor="textSecondary"> cm</ThemedText>
        </View>
        <ThemedText type="small" themeColor="textSecondary">
          {!connected
            ? `no link: ${error?.message ?? 'connecting...'}`
            : `${state!.dir > 0 ? 'moving up' : state!.dir < 0 ? 'moving down' : state!.travel ? 'travelling to a memory position' : state!.moving ? 'moving' : 'still'} · source: ${state!.src}${state!.disp ? ` · display "${state!.disp}"` : ''}${state!.armed ? '' : ' · LOCKED'}`}
        </ThemedText>
        {state && state.src === 'estimate' ? (
          <ThemedText type="small" style={{ color: '#FF9F0A' }}>
            The display cannot be read, so the height is only estimated. Enter what the desk display shows to calibrate.
          </ThemedText>
        ) : null}
        {state && state.src === 'none' ? (
          <ThemedText type="small" style={{ color: '#FF9F0A' }}>
            Height unknown. Enter what the desk display shows.
          </ThemedText>
        ) : null}
        <View style={styles.row}>
          <TextInput
            value={calib}
            onChangeText={setCalib}
            keyboardType="decimal-pad"
            placeholder="display shows, e.g. 82.0"
            placeholderTextColor={theme.textSecondary}
            style={[styles.input, styles.flex, { color: theme.text, backgroundColor: theme.backgroundSelected }]}
          />
          <Chip label="Set" onPress={() => send((c) => c.setHeight(parseFloat(calib.replace(',', '.'))))} />
        </View>
      </Card>

      <Card title="Move (hold the button)">
        <View style={styles.row}>
          <HoldButton label="▲  UP" dir="up" onMove={(d) => void client.move(d).catch((e: Error) => setError(e.message))} onStop={() => void client.stop().catch(() => {})} />
          <HoldButton label="▼  DOWN" dir="down" onMove={(d) => void client.move(d).catch((e: Error) => setError(e.message))} onStop={() => void client.stop().catch(() => {})} />
        </View>
        <Button label="STOP" color="#FF3B30" onPress={() => send((c) => c.stop())} />
        <ThemedText type="small" themeColor="textSecondary">
          The desk only moves while you hold the button and the link works: if the connection drops it stops by itself within half a second. A single run never lasts longer than 25 s.
        </ThemedText>
      </Card>

      <Card title="Memory positions">
        <Wrap>
          {[1, 2, 3, 4].map((n) => (
            <Chip key={n} label={`Go to ${n}`} onPress={() => confirm(`Go to position ${n}`, 'The desk will drive to the stored height. Make sure nothing is in the way. You can press STOP.', () => send((c) => c.preset(n)))} />
          ))}
        </Wrap>
        <Wrap>
          {[1, 2, 3, 4].map((n) => (
            <Chip key={n} label={`Save as ${n}`} color="#8E8E93" onPress={() => confirm(`Save position ${n}`, 'Stores the CURRENT height in memory ' + n + ' (presses S, then ' + n + ').', () => send((c) => c.saveMemory(n)))} />
          ))}
        </Wrap>
        <SwitchRow
          label="Memory key must be held"
          hint="Turn on if the desk only travels while its memory key stays pressed"
          value={!!state?.phold}
          onChange={(v) => send((c) => c.setPresetHold(v))}
        />
      </Card>

      <Card title="Safety">
        <SwitchRow label="Child lock" hint="Refuses every movement command (the handset still works)" value={state ? !state.armed : false} onChange={(v) => send((c) => c.arm(!v))} />
      </Card>

      <Card title="Robots">
        <ThemedText type="small" themeColor="textSecondary">
          Each robot pairs with the desk on its own (own key). Do this while the pairing window is open (power-cycle the desk first). Only this phone can then use the desk functions of the robot (secret token).
        </ThemedText>
        {robot ? (
          <>
            <Stat
              label={`${robot.name} and the desk`}
              value={!robotState?.desk ? 'robot has no desk support (update firmware)' : robotState.desk.paired ? `paired · ${robotState.desk.ok ? 'link ok' : `no link ${robotState.desk.err}`}` : 'not paired'}
            />
            <Wrap>
              <Button label={`Pair ${robot.name} with desk`} onPress={() => sendRobot((c) => c.deskPair(host ?? 'desk.local', token ?? ''))} />
              <Button label="Unpair robot" color="#8E8E93" onPress={() => sendRobot((c) => c.deskUnpair(token ?? ''))} />
            </Wrap>
          </>
        ) : (
          <ThemedText type="small">Select a robot on the Robots tab first.</ThemedText>
        )}
      </Card>

      <Card title="Diagnostics">
        <Wrap gap={Spacing.three}>
          <Stat label="Firmware" value={state?.fw ?? '-'} />
          <Stat label="Wi-Fi" value={state ? `${state.rssi} dBm` : '-'} />
          <Stat label="Keys pressed" value={state?.keys ?? '-'} />
          <Stat label="Serial bytes" value={state ? String(state.bytes) : '-'} />
          <Stat label="Frames ok / bad" value={state ? `${state.frames} / ${state.bad}` : '-'} />
        </Wrap>
        <Wrap>
          <Chip label="Desk log" onPress={async () => { const r = await client.log(0).catch((e: Error) => { setError(e.message); return undefined; }); if (r) setLines(r.lines); }} />
          <Chip label="Raw capture" onPress={async () => { const r = await client.raw().catch((e: Error) => { setError(e.message); return undefined; }); if (r) setRaw(r.text || '(nothing captured)'); }} />
          <Chip label="Clients" onPress={async () => { const r = await client.clients().catch((e: Error) => { setError(e.message); return undefined; }); if (r) setRaw(r.clients.map((c) => `${c.id} ${c.name}${c.you ? ' (this phone)' : ''}`).join('\n')); }} />
        </Wrap>
        {raw ? <ThemedText type="code" selectable>{raw}</ThemedText> : null}
        {lines.slice(-40).reverse().map((l) => (
          <ThemedText key={l.n} type="code" selectable style={{ color: l.l === 0 ? '#FF3B30' : l.l === 1 ? '#FF9F0A' : theme.text }}>
            {(l.t / 1000).toFixed(1)}s {l.m}
          </ThemedText>
        ))}
      </Card>

      <Card title="Pairing">
        <Button
          label="Forget this desk (phone side)"
          color="#FF3B30"
          onPress={() => confirm('Forget the desk', 'Removes the key from this phone and unpairs it on the desk module.', async () => { await client.unpair().catch(() => {}); await forget(); })}
        />
      </Card>
    </>
  );
}

const styles = StyleSheet.create({
  flex: { flex: 1 },
  row: { flexDirection: 'row', gap: Spacing.two, alignItems: 'center' },
  heightRow: { flexDirection: 'row', alignItems: 'baseline' },
  input: { borderRadius: 12, paddingHorizontal: 14, paddingVertical: 12, fontSize: 16 },
  hold: { flex: 1, height: 92, borderRadius: 18, alignItems: 'center', justifyContent: 'center' },
  holdText: { fontSize: 22, fontWeight: '800' },
});

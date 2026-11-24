// Console: type commands and see the answers, the robot's own log, its events, and every request the app sent.
import { useMemo, useState } from 'react';
import { Share, StyleSheet, TextInput, View } from 'react-native';

import { PAD_NAMES } from '@/api/catalog';
import { ThemedText } from '@/components/themed-text';
import { Button, Card, Chip, Page, Segmented, Wrap } from '@/components/ui';
import { Spacing } from '@/constants/theme';
import { useActiveRobot, useRobotState } from '@/hooks/use-robot';
import { useTheme } from '@/hooks/use-theme';
import { runDesk } from '@/lib/desk-commands';
import { runCommand } from '@/lib/commands';
import { describeEvent, fmtTime } from '@/lib/events';
import { useConsole, useLogs, useTraffic } from '@/store/diagnostics';
import { useUi } from '@/store/robots';

type View_ = 'commands' | 'log' | 'events' | 'traffic';

const QUICK = ['help', 'state', 'diag', 'desk state', 'mode manual', 'mode auto', 'mood happy', 'move wiggle', 'stop', 'say hello', 'beep 440 300', 'release', 'pad 4 double'];
const LEVEL_COLOR = ['#FF3B30', '#FF9F0A', undefined, '#8E8E93'] as const;
const LEVEL_CHAR = ['E', 'W', 'I', 'D'];

function Commands() {
  const theme = useTheme();
  const { client } = useActiveRobot();
  const entries = useConsole((s) => s.entries);
  const add = useConsole((s) => s.add);
  const clear = useConsole((s) => s.clear);
  const [text, setText] = useState('');
  const [busy, setBusy] = useState(false);

  const send = async (cmd: string) => {
    if (!cmd.trim()) return;
    setBusy(true);
    const t0 = Date.now();
    try {
      const r = await runCommand(client, cmd, runDesk);
      add({ cmd, ok: r.ok, text: r.text, ms: Date.now() - t0 });
    } catch (e) {
      add({ cmd, ok: false, text: (e as Error).message, ms: Date.now() - t0 });
    } finally {
      setBusy(false);
      setText('');
    }
  };

  return (
    <>
      <Card title="Send a command">
        <TextInput
          value={text}
          onChangeText={setText}
          onSubmitEditing={() => send(text)}
          placeholder="e.g.  mood happy   or   POST /api/beep?hz=440&ms=300"
          placeholderTextColor={theme.textSecondary}
          autoCapitalize="none"
          autoCorrect={false}
          returnKeyType="send"
          style={[styles.input, { color: theme.text, backgroundColor: theme.backgroundSelected }]}
        />
        <Button label={busy ? 'Sending...' : 'Send'} disabled={busy} onPress={() => send(text)} />
        <Wrap>
          {QUICK.map((q) => (
            <Chip key={q} label={q} onPress={() => (q.includes(' ') || q === 'help' || q === 'state' || q === 'diag' || q === 'stop' || q === 'release' ? send(q) : setText(q))} />
          ))}
        </Wrap>
        <ThemedText type="small" themeColor="textSecondary">
          Type help for all commands. Any request works: GET /api/state, POST /api/mood?m=3 ...
        </ThemedText>
      </Card>
      <Card title="Answers">
        <Wrap>
          <Chip label="Clear" color="#8E8E93" onPress={clear} />
          <Chip label="Share" color="#8E8E93" onPress={() => Share.share({ message: entries.map((e) => `> ${e.cmd}\n${e.ok ? 'OK' : 'FAIL'} (${e.ms} ms)\n${e.text}`).join('\n\n') })} />
        </Wrap>
        {entries.length === 0 ? <ThemedText themeColor="textSecondary">Nothing sent yet.</ThemedText> : null}
        {[...entries].reverse().map((e) => (
          <View key={e.id} style={styles.entry}>
            <ThemedText type="smallBold">
              <ThemedText type="smallBold" style={{ color: e.ok ? '#34C759' : '#FF3B30' }}>{e.ok ? '✓ ' : '✗ '}</ThemedText>
              {e.cmd}  <ThemedText type="small" themeColor="textSecondary">{e.ms} ms</ThemedText>
            </ThemedText>
            {e.text ? <ThemedText type="code" selectable>{e.text}</ThemedText> : null}
          </View>
        ))}
      </Card>
    </>
  );
}

function RobotLog() {
  const theme = useTheme();
  const lines = useLogs((s) => s.lines);
  const clear = useLogs((s) => s.clear);
  const [level, setLevel] = useState(3);
  const [query, setQuery] = useState('');
  const [paused, setPaused] = useState(false);
  const [frozen, setFrozen] = useState<typeof lines>([]);

  const source = paused ? frozen : lines;
  const shown = useMemo(() => {
    const q = query.trim().toLowerCase();
    return source
      .filter((l) => l.l <= level && (!q || l.m.toLowerCase().includes(q)))
      .slice(-250)
      .reverse();
  }, [source, level, query]);

  return (
    <Card title={`Robot log (${lines.length} lines kept, newest first)`}>
      <Segmented
        options={[
          { value: '0', label: 'Errors' },
          { value: '1', label: 'Warn+' },
          { value: '2', label: 'Info+' },
          { value: '3', label: 'All' },
        ]}
        value={String(level)}
        onChange={(v) => setLevel(Number(v))}
      />
      <TextInput
        value={query}
        onChangeText={setQuery}
        placeholder="search (e.g. pad, servo, wifi)"
        placeholderTextColor={theme.textSecondary}
        autoCapitalize="none"
        style={[styles.input, { color: theme.text, backgroundColor: theme.backgroundSelected }]}
      />
      <Wrap>
        <Chip
          label={paused ? 'Resume' : 'Pause'}
          active={paused}
          onPress={() => {
            if (!paused) setFrozen(lines);
            setPaused(!paused);
          }}
        />
        <Chip label="Clear" color="#8E8E93" onPress={clear} />
        <Chip label="Share all" color="#8E8E93" onPress={() => Share.share({ message: lines.map((l) => `${l.local ? '--:--' : fmtTime(l.t)} ${LEVEL_CHAR[l.l]} ${l.m}`).join('\n') })} />
      </Wrap>
      {shown.length === 0 ? <ThemedText themeColor="textSecondary">No log lines yet. Is a robot connected?</ThemedText> : null}
      {shown.map((l) => (
        <ThemedText key={l.n} type="code" selectable style={{ color: LEVEL_COLOR[l.l] ?? theme.text }}>
          {l.local ? '  --  ' : fmtTime(l.t)} {LEVEL_CHAR[l.l]} {l.m}
        </ThemedText>
      ))}
    </Card>
  );
}

function Events() {
  const events = useUi((s) => s.events);
  const clear = useUi((s) => s.clearEvents);
  const { state } = useRobotState();
  const pads = PAD_NAMES[state?.robot ?? 'chiku'];
  return (
    <Card title="Robot events (touch, sensors, buttons)">
      <Wrap>
        <Chip label="Clear" color="#8E8E93" onPress={clear} />
        <Chip label="Share" color="#8E8E93" onPress={() => Share.share({ message: events.map((e) => `${fmtTime(e.t)} ${describeEvent(e, pads)}`).join('\n') })} />
      </Wrap>
      {events.length === 0 ? <ThemedText themeColor="textSecondary">Touch a pad, tap the screen, or use Test, then Simulate.</ThemedText> : null}
      {[...events].reverse().slice(0, 150).map((e) => (
        <View key={e.n} style={styles.logRow}>
          <ThemedText type="code" themeColor="textSecondary">{fmtTime(e.t)}</ThemedText>
          <ThemedText type="small" style={styles.flex}>{describeEvent(e, pads)}</ThemedText>
        </View>
      ))}
    </Card>
  );
}

function Traffic() {
  const items = useTraffic((s) => s.items);
  const clear = useTraffic((s) => s.clear);
  const slow = items.filter((i) => i.ms > 500).length;
  const failed = items.filter((i) => !i.ok).length;
  return (
    <Card title="App to robot requests">
      <ThemedText type="small" themeColor="textSecondary">
        {items.length} requests, {failed} failed, {slow} slower than 500 ms.
      </ThemedText>
      <Wrap>
        <Chip label="Clear" color="#8E8E93" onPress={clear} />
        <Chip
          label="Share"
          color="#8E8E93"
          onPress={() => Share.share({ message: items.map((i) => `${new Date(i.at).toISOString()} ${i.host} ${i.method} ${i.path} -> ${i.ok ? 'OK' : 'FAIL'} ${i.status ?? ''} ${i.ms} ms ${i.error ?? i.response ?? ''}`).join('\n') })}
        />
      </Wrap>
      {[...items].reverse().slice(0, 150).map((i) => (
        <View key={i.id} style={styles.entry}>
          <ThemedText type="small">
            <ThemedText type="smallBold" style={{ color: i.ok ? '#34C759' : '#FF3B30' }}>{i.ok ? '✓ ' : '✗ '}</ThemedText>
            <ThemedText type="smallBold">{i.method} {i.path}</ThemedText>
            <ThemedText type="small" themeColor="textSecondary">  {i.status ?? '-'} · {i.ms} ms</ThemedText>
          </ThemedText>
          {i.body ? <ThemedText type="code" themeColor="textSecondary">body: {i.body.slice(0, 160)}</ThemedText> : null}
          <ThemedText type="code" selectable style={{ color: i.ok ? undefined : '#FF3B30' }}>{(i.error ?? i.response ?? '').slice(0, 200)}</ThemedText>
        </View>
      ))}
    </Card>
  );
}

export default function ConsoleScreen() {
  const [view, setView] = useState<View_>('commands');
  return (
    <Page title="Console">
      <Segmented<View_>
        options={[
          { value: 'commands', label: 'Commands' },
          { value: 'log', label: 'Robot log' },
          { value: 'events', label: 'Events' },
          { value: 'traffic', label: 'Requests' },
        ]}
        value={view}
        onChange={setView}
      />
      {view === 'commands' ? <Commands /> : null}
      {view === 'log' ? <RobotLog /> : null}
      {view === 'events' ? <Events /> : null}
      {view === 'traffic' ? <Traffic /> : null}
    </Page>
  );
}

const styles = StyleSheet.create({
  flex: { flex: 1 },
  input: { borderRadius: 12, paddingHorizontal: 14, paddingVertical: 12, fontSize: 16 },
  entry: { gap: 2, paddingVertical: Spacing.one },
  logRow: { flexDirection: 'row', gap: Spacing.two, alignItems: 'center' },
});

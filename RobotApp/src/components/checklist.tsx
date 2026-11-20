// The hardware checklist screen: every sensor, pad, motor, screen and sound with a Run button and a result.
import { ActivityIndicator, Share, StyleSheet, View } from 'react-native';

import type { TestDef } from '@/lib/checklist';
import { ThemedText } from '@/components/themed-text';
import { Button, Card, Chip, Wrap } from '@/components/ui';
import { Spacing } from '@/constants/theme';
import { useChecklistRunner } from '@/hooks/use-checklist';
import { useTheme } from '@/hooks/use-theme';
import type { TestResult } from '@/store/diagnostics';

const KIND_LABEL: Record<TestDef['kind'], string> = { auto: 'automatic', watch: 'you act', confirm: 'you confirm' };

function icon(r?: TestResult) {
  if (!r) return { sym: '○', color: '#8E8E93' };
  if (r.status === 'pass') return { sym: '✓', color: '#34C759' };
  if (r.status === 'fail') return { sym: '✗', color: '#FF3B30' };
  return { sym: '…', color: '#FF9F0A' };
}

function Row({ t, r, onRun, onAnswer }: { t: TestDef; r?: TestResult; onRun: () => void; onAnswer: (ok: boolean) => void }) {
  const theme = useTheme();
  const ic = icon(r);
  const running = r?.status === 'running';
  return (
    <View style={[styles.row, { borderColor: theme.backgroundSelected }]}>
      <View style={styles.rowTop}>
        <ThemedText type="smallBold" style={{ color: ic.color, width: 18 }}>
          {ic.sym}
        </ThemedText>
        <View style={styles.flex}>
          <ThemedText type="smallBold">{t.title}</ThemedText>
          <ThemedText type="small" themeColor="textSecondary">
            {KIND_LABEL[t.kind]}
          </ThemedText>
        </View>
        {running ? <ActivityIndicator /> : <Chip label={r ? 'Run again' : 'Run'} onPress={onRun} />}
      </View>
      {running || !r ? (
        <ThemedText type="small" themeColor="textSecondary">
          {running ? r.detail : t.help}
        </ThemedText>
      ) : r.status === 'asking' ? (
        <View style={styles.ask}>
          <ThemedText type="small">{r.ask}</ThemedText>
          <Wrap>
            <Chip label="Yes, it worked" color="#34C759" active onPress={() => onAnswer(true)} />
            <Chip label="No / wrong" color="#FF3B30" active onPress={() => onAnswer(false)} />
          </Wrap>
        </View>
      ) : (
        <ThemedText type="small" style={{ color: ic.color }}>
          {r.detail}
        </ThemedText>
      )}
    </View>
  );
}

export function Checklist() {
  const { tests, resultOf, run, answer, runAuto, report, reset, enabled } = useChecklistRunner();
  const counts = tests.reduce(
    (a, t) => {
      const s = resultOf(t.id)?.status;
      if (s === 'pass') a.pass++;
      else if (s === 'fail') a.fail++;
      else a.todo++;
      return a;
    },
    { pass: 0, fail: 0, todo: 0 },
  );
  const groups = [...new Set(tests.map((t) => t.group))];

  return (
    <>
      <Card title="Hardware checklist">
        <ThemedText type="small" themeColor="textSecondary">
          One test for every screen, sensor, touch pad, motor and sound. Put the robot in Manual mode first (Control tab). Tests marked you act wait for you
          to touch or move something; you confirm tests make the robot do something and ask what you saw or heard.
        </ThemedText>
        <ThemedText type="subtitle">
          <ThemedText type="subtitle" style={{ color: '#34C759' }}>{counts.pass} ok</ThemedText>
          {'  '}
          <ThemedText type="subtitle" style={{ color: '#FF3B30' }}>{counts.fail} failed</ThemedText>
          {'  '}
          <ThemedText type="subtitle" themeColor="textSecondary">{counts.todo} open</ThemedText>
        </ThemedText>
        <Wrap>
          <Button label="Run automatic tests" disabled={!enabled} onPress={runAuto} />
          <Button label="Share report" color="#8E8E93" onPress={() => Share.share({ message: report() })} />
          <Button label="Clear results" color="#FF9F0A" onPress={reset} />
        </Wrap>
      </Card>

      {groups.map((g) => (
        <Card key={g} title={g}>
          {tests
            .filter((t) => t.group === g)
            .map((t) => (
              <Row key={t.id} t={t} r={resultOf(t.id)} onRun={() => run(t)} onAnswer={(ok) => answer(t, ok)} />
            ))}
        </Card>
      ))}
    </>
  );
}

const styles = StyleSheet.create({
  flex: { flex: 1 },
  row: { gap: 6, paddingTop: Spacing.two, borderTopWidth: StyleSheet.hairlineWidth },
  rowTop: { flexDirection: 'row', alignItems: 'center', gap: Spacing.two },
  ask: { gap: 6 },
});

// Checks the app's hub client against a running Server/server.py:  npx tsx tools/smoke-hub.mts localhost:8080
import { HubClient, normalizeHubUrl } from '../src/hub/client';

const base = normalizeHubUrl(process.argv[2] ?? 'localhost:8080');
const hub = new HubClient(base, 'chiku');
let failed = 0;
const check = (name: string, ok: boolean) => {
  console.log(`${ok ? 'PASS' : 'FAIL'} ${name}`);
  if (!ok) failed++;
};

const s = await hub.state();
check('state has devices', s.devices.length > 0);
const lamp = s.devices.find((d) => d.type === 'light')!;
await hub.setDevice(lamp.id, { level: 33, on: true });
check('device level changes', (await hub.state()).devices.find((d) => d.id === lamp.id)!.level === 33);
check('robot settings accepted', (await hub.setRobot('chiku', { clapSens: 70 })).ok);
check('history is a list', Array.isArray(await hub.history('chiku')));
check('url normalised', normalizeHubUrl('192.168.1.2:8080/') === 'http://192.168.1.2:8080');
let denied = false;
try {
  await new HubClient(base, 'wrong').setDevice(lamp.id, { on: false });
} catch {
  denied = true;
}
check('wrong token refused', denied);
process.exit(failed ? 1 : 0);

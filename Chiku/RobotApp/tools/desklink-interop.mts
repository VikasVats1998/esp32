// Interop test: the app's TypeScript crypto must agree byte for byte with the firmware's C++ code
// (Shared/DeskLink). Needs the native tool:
//   g++ -std=c++17 -o /tmp/desklink_tool ../Shared/DeskLink/test/desklink_tool.cpp ../Shared/DeskLink/src/desklink.cpp -lmbedcrypto
//   DESKLINK_TOOL=/tmp/desklink_tool npx tsx tools/desklink-interop.mts
import { execFileSync } from 'node:child_process';

import { deriveKey, fromHex, generateKeyPair, open, seal, toHex } from '../src/desk/crypto';

const tool = process.env.DESKLINK_TOOL ?? '/tmp/desklink_tool';
const run = (...args: string[]) => execFileSync(tool, args, { encoding: 'utf8' }).trim();
let failed = 0;
const check = (name: string, ok: boolean, extra = '') => {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${name} ${extra}`);
  if (!ok) failed++;
};

// 1. same public key from the same private key
const mine = generateKeyPair();
check('public key from private key matches C++', run('pub', toHex(mine.priv)) === toHex(mine.pub));

// 2. ECDH + HKDF: C++ server side and TypeScript client side derive the same key
const [serverPriv, serverPub] = run('keygen').split(' ');
const key = deriveKey(mine, fromHex(serverPub), mine.pub, fromHex(serverPub));
const keyCpp = run('derive', serverPriv, toHex(mine.pub), toHex(mine.pub), serverPub);
check('ECDH + HKDF key agrees (client TS, server C++)', toHex(key) === keyCpp);

// 3. TypeScript seals, C++ opens
const chal = crypto.getRandomValues(new Uint8Array(16));
const msg = '{"op":"move","dir":"up","ms":400}';
const blob = seal(key, chal, 'cmd', msg);
check('C++ opens what TS sealed', run('open', keyCpp, toHex(chal), 'cmd', toHex(blob)) === msg);

// 4. C++ seals, TypeScript opens
const reply = '{"ok":true,"h":82.0}';
const blob2 = run('seal', keyCpp, toHex(chal), 'rsp', reply);
check('TS opens what C++ sealed', open(key, chal, 'rsp', fromHex(blob2)) === reply);

// 5. protection: wrong purpose, wrong challenge, tampering must all fail on both sides
let rejected = 0;
try { open(key, chal, 'cmd', fromHex(blob2)); } catch { rejected++; }
const other = chal.slice(); other[0] ^= 1;
try { open(key, other, 'rsp', fromHex(blob2)); } catch { rejected++; }
const bad = fromHex(blob2); bad[15] ^= 1;
try { open(key, chal, 'rsp', bad); } catch { rejected++; }
check('TS rejects wrong purpose / challenge / tampering', rejected === 3);
let cppRejected = false;
try { run('open', keyCpp, toHex(other), 'cmd', toHex(blob)); } catch { cppRejected = true; }
check('C++ rejects wrong challenge', cppRejected);

console.log(failed ? `\n${failed} FAILED` : '\nall passed');
process.exit(failed ? 1 : 0);

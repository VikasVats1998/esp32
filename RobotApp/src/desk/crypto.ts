// DeskLink v1 crypto for the app (same protocol as Shared/DeskLink/src/desklink.cpp).
// ECDH P-256 + HKDF-SHA256 for pairing, AES-256-GCM for sealed messages bound to a one-time challenge.
// Pure TypeScript (noble libraries): runs in React Native and in Node (tests).
import { gcm } from '@noble/ciphers/aes.js';
import { p256 } from '@noble/curves/nist.js';
import { hkdf } from '@noble/hashes/hkdf.js';
import { sha256 } from '@noble/hashes/sha2.js';
import { bytesToHex, hexToBytes } from '@noble/hashes/utils.js';

export const IV_LEN = 12;
export const CHALLENGE_LEN = 16;

type RandomFn = (n: number) => Uint8Array;
let randomFn: RandomFn = (n) => {
  const c = (globalThis as { crypto?: { getRandomValues?: (a: Uint8Array) => Uint8Array } }).crypto;
  if (!c?.getRandomValues) throw new Error('No secure random source: call setRandomProvider()');
  return c.getRandomValues(new Uint8Array(n));
};

/** The app sets this to expo-crypto's getRandomBytes (React Native has no crypto.getRandomValues). */
export function setRandomProvider(fn: RandomFn) {
  randomFn = fn;
}

export const toHex = (b: Uint8Array) => bytesToHex(b);
/** n random bytes as hex (e.g. access tokens). */
export const randomHex = (n: number) => bytesToHex(randomFn(n));
export const fromHex = (h: string) => hexToBytes(h);

export interface KeyPair {
  priv: Uint8Array;
  /** uncompressed point, 65 bytes */
  pub: Uint8Array;
}

export function generateKeyPair(): KeyPair {
  const priv = p256.utils.randomSecretKey(randomFn(48));
  return { priv, pub: p256.getPublicKey(priv, false) };
}

/** Key = HKDF-SHA256(ECDH secret, salt "desklink-v1", info = clientPub || serverPub). */
export function deriveKey(mine: KeyPair, peerPub: Uint8Array, clientPub: Uint8Array, serverPub: Uint8Array): Uint8Array {
  const shared = p256.getSharedSecret(mine.priv, peerPub, true).slice(1); // x coordinate
  const info = new Uint8Array(clientPub.length + serverPub.length);
  info.set(clientPub, 0);
  info.set(serverPub, clientPub.length);
  return hkdf(sha256, shared, new TextEncoder().encode('desklink-v1'), info, 32);
}

const aad = (challenge: Uint8Array, purpose: string) => {
  const p = new TextEncoder().encode(purpose);
  const a = new Uint8Array(challenge.length + p.length);
  a.set(challenge, 0);
  a.set(p, challenge.length);
  return a;
};

/** Returns iv(12) || ciphertext || tag(16). */
export function seal(key: Uint8Array, challenge: Uint8Array, purpose: 'cmd' | 'rsp', plain: string): Uint8Array {
  const iv = randomFn(IV_LEN);
  const ct = gcm(key, iv, aad(challenge, purpose)).encrypt(new TextEncoder().encode(plain));
  const out = new Uint8Array(IV_LEN + ct.length);
  out.set(iv, 0);
  out.set(ct, IV_LEN);
  return out;
}

/** Throws if the message was changed, belongs to another challenge, or has the wrong purpose. */
export function open(key: Uint8Array, challenge: Uint8Array, purpose: 'cmd' | 'rsp', blob: Uint8Array): string {
  const iv = blob.slice(0, IV_LEN);
  const plain = gcm(key, iv, aad(challenge, purpose)).decrypt(blob.slice(IV_LEN));
  return new TextDecoder().decode(plain);
}

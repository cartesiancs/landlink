import { beforeEach, describe, expect, it } from "vitest";

import {
  _resetPkiStore,
  findPublicKey,
  getPublicKeys,
  recordPublicKey,
  subscribePublicKeys,
} from "./store";

function makeKey(seed: number): Uint8Array {
  const out = new Uint8Array(32);
  for (let i = 0; i < 32; i++) out[i] = (seed + i) & 0xff;
  return out;
}

describe("meshtastic-pki store", () => {
  beforeEach(() => {
    _resetPkiStore();
  });

  it("records and finds a 32-byte public key", () => {
    const key = makeKey(7);
    recordPublicKey(0x11223344, key);
    const found = findPublicKey(0x11223344);
    expect(found).not.toBeNull();
    expect(Array.from(found ?? new Uint8Array(0))).toEqual(Array.from(key));
  });

  it("returns null for unknown node id", () => {
    expect(findPublicKey(0xdeadbeef)).toBeNull();
    expect(findPublicKey(null)).toBeNull();
  });

  it("rejects keys with the wrong length", () => {
    recordPublicKey(0x11223344, new Uint8Array(16));
    recordPublicKey(0x11223344, new Uint8Array(33));
    expect(findPublicKey(0x11223344)).toBeNull();
  });

  it("overwrites with the latest key when the same node sends a new one", () => {
    recordPublicKey(0xaabbccdd, makeKey(1));
    recordPublicKey(0xaabbccdd, makeKey(2));
    const found = findPublicKey(0xaabbccdd);
    expect(Array.from(found ?? new Uint8Array(0))).toEqual(
      Array.from(makeKey(2)),
    );
  });

  it("notifies subscribers when a new key arrives", () => {
    let fired = 0;
    const unsubscribe = subscribePublicKeys(() => {
      fired++;
    });
    recordPublicKey(0xaaaaaaaa, makeKey(3));
    expect(fired).toBe(1);
    recordPublicKey(0xaaaaaaaa, makeKey(3));
    expect(fired).toBe(1);
    unsubscribe();
  });

  it("exposes a snapshot map that includes recorded entries", () => {
    recordPublicKey(0x11111111, makeKey(4));
    recordPublicKey(0x22222222, makeKey(5));
    const snapshot = getPublicKeys();
    expect(snapshot.size).toBe(2);
    expect(snapshot.has(0x11111111)).toBe(true);
    expect(snapshot.has(0x22222222)).toBe(true);
  });
});

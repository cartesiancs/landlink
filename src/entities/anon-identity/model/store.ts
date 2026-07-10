// In-memory anonymous-identity store. Mirrors the hand-rolled external-store
// pattern used across the app (module state + listener set + useSyncExternalStore
// in the hook). The full StoredIdentity (including the non-extractable keypair)
// stays in module memory; only the serialization-safe AnonIdentity is exposed
// through the snapshot.

import {
  createStoredIdentity,
  deleteStoredIdentity,
  loadStoredIdentity,
  signWithIdentity,
  type StoredIdentity,
} from "../api/identity-store";
import type { AnonIdentity } from "./types";

let stored: StoredIdentity | null = null;
let snapshot: AnonIdentity | null = null;
let loaded = false;
let loadPromise: Promise<AnonIdentity | null> | null = null;
const listeners = new Set<() => void>();

function emit(): void {
  for (const l of listeners) {
    try {
      l();
    } catch {
      // listeners must not break each other
    }
  }
}

function setStored(next: StoredIdentity | null): void {
  stored = next;
  snapshot = next ? { accountId: next.accountId, publicKeyRaw: next.publicKeyRaw } : null;
  loaded = true;
  emit();
}

export function getAnonIdentitySnapshot(): AnonIdentity | null {
  return snapshot;
}

export function isAnonIdentityLoaded(): boolean {
  return loaded;
}

export function subscribeAnonIdentity(cb: () => void): () => void {
  listeners.add(cb);
  return () => {
    listeners.delete(cb);
  };
}

// Load the persisted identity if present. Never creates one. Idempotent — the
// in-flight promise is shared so concurrent hook mounts don't double-read IDB.
export function loadAnonIdentity(): Promise<AnonIdentity | null> {
  if (loaded) return Promise.resolve(snapshot);
  loadPromise ??= (async () => {
    try {
      const found = await loadStoredIdentity();
      setStored(found);
    } catch (err) {
      console.warn("[anon-identity] load failed", err);
      setStored(null);
    }
    loadPromise = null;
    return snapshot;
  })();
  return loadPromise;
}

// Ensure an identity exists, creating one on first call. This is the
// "register anonymous account" action — no server round-trip, no PII.
export async function ensureAnonIdentity(): Promise<AnonIdentity> {
  const existing = await loadAnonIdentity();
  if (existing) return existing;
  const created = await createStoredIdentity();
  setStored(created);
  return { accountId: created.accountId, publicKeyRaw: created.publicKeyRaw };
}

// Sign a relay challenge nonce. Throws if no identity has been created yet.
export async function signChallenge(nonce: Uint8Array): Promise<Uint8Array> {
  let identity = stored;
  if (!identity) {
    await loadAnonIdentity();
    identity = stored;
  }
  if (!identity) {
    throw new Error("No anonymous identity. Register an account first.");
  }
  return signWithIdentity(identity, nonce);
}

// A structural signer for the relay handshake / enrollment. Matches the
// remote-session RelaySigner shape without this entity importing that one
// (entities may not import each other). Returns null until an identity exists.
export function getAnonSigner():
  | { publicKeyRaw: Uint8Array; sign: (nonce: Uint8Array) => Promise<Uint8Array> }
  | null {
  if (!snapshot) return null;
  const publicKeyRaw = snapshot.publicKeyRaw;
  return { publicKeyRaw, sign: (nonce) => signChallenge(nonce) };
}

export async function resetAnonIdentity(): Promise<void> {
  try {
    await deleteStoredIdentity();
  } catch (err) {
    console.warn("[anon-identity] delete failed", err);
  }
  setStored(null);
}

export function _resetAnonIdentityStore(): void {
  stored = null;
  snapshot = null;
  loaded = false;
  loadPromise = null;
  listeners.clear();
}

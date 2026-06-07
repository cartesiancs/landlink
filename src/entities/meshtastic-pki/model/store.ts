// In-memory cache of node X25519 public keys learned from NodeInfo broadcasts.
// STEP 1 scope: visibility only. Keys are not used for crypto here — when the
// firmware/STEP 2 layer starts performing PKI encrypt/decrypt, this store is
// the lookup table they'll consult.

const PUBLIC_KEY_BYTES = 32;

const keys = new Map<string, Uint8Array>();
const listeners = new Set<() => void>();
let snapshot: ReadonlyMap<string, Uint8Array> = new Map();

function rebuildSnapshot(): void {
  // The snapshot reference must change for React's useSyncExternalStore to
  // re-render consumers; cloning the Map is cheap relative to NodeInfo cadence.
  snapshot = new Map(keys);
}

function emit(): void {
  rebuildSnapshot();
  for (const cb of listeners) {
    try {
      cb();
    } catch {
      // listeners must not break each other
    }
  }
}

export function recordPublicKey(nodeId: string, key: Uint8Array): void {
  if (!nodeId) return;
  if (key.byteLength !== PUBLIC_KEY_BYTES) {
    console.warn("[meshtastic-pki] reject non-32B public key", {
      nodeId,
      length: key.byteLength,
    });
    return;
  }
  const existing = keys.get(nodeId);
  if (existing && bytesEqual(existing, key)) return;
  keys.set(nodeId, key.slice());
  emit();
}

export function findPublicKey(nodeId: string | null): Uint8Array | null {
  if (!nodeId) return null;
  return keys.get(nodeId) ?? null;
}

export function getPublicKeys(): ReadonlyMap<string, Uint8Array> {
  return snapshot;
}

export function subscribePublicKeys(cb: () => void): () => void {
  listeners.add(cb);
  return () => {
    listeners.delete(cb);
  };
}

export function _resetPkiStore(): void {
  keys.clear();
  snapshot = new Map();
}

function bytesEqual(a: Uint8Array, b: Uint8Array): boolean {
  if (a.byteLength !== b.byteLength) return false;
  for (let i = 0; i < a.byteLength; i++) {
    if (a[i] !== b[i]) return false;
  }
  return true;
}

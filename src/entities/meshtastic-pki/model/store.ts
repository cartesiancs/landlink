// In-memory cache of node X25519 public keys learned from NodeInfo
// broadcasts. STEP 1 scope: visibility only. Keys are not used for crypto
// here; firmware owns the keypair and decides PKI vs PSK on send. The host
// uses this cache to decide which UI badge to show in the DM composer.

const PUBLIC_KEY_BYTES = 32;

// Keyed by numeric nodeNum so this store is consistent with lora-peer and
// landlink-device, which now identify peers numerically.
const keys = new Map<number, Uint8Array>();
const listeners = new Set<() => void>();
let snapshot: ReadonlyMap<number, Uint8Array> = new Map();

function rebuildSnapshot(): void {
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

export function recordPublicKey(nodeNum: number, key: Uint8Array): void {
  if (!Number.isFinite(nodeNum)) return;
  if (key.byteLength !== PUBLIC_KEY_BYTES) {
    console.warn("[meshtastic-pki] reject non-32B public key", {
      nodeNum,
      length: key.byteLength,
    });
    return;
  }
  const existing = keys.get(nodeNum);
  if (existing && bytesEqual(existing, key)) return;
  keys.set(nodeNum, key.slice());
  emit();
}

export function findPublicKey(nodeNum: number | null): Uint8Array | null {
  if (nodeNum === null) return null;
  return keys.get(nodeNum) ?? null;
}

export function getPublicKeys(): ReadonlyMap<number, Uint8Array> {
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

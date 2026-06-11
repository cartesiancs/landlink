// In-memory cache of node X25519 public keys learned from NodeInfo
// broadcasts, with a localStorage backing so a page reload doesn't blank the
// PKI badge state while we wait for fresh NodeInfo packets. Firmware owns
// the keypair and decides PKI vs PSK on send; the host uses this cache to
// drive the DM composer UI and to determine when an explicit NodeInfo
// request is needed before sending a DM.

const PUBLIC_KEY_BYTES = 32;
const STORAGE_KEY = "meshtastic-pki:v1";
const SAVE_DEBOUNCE_MS = 300;

// Keyed by numeric nodeNum so this store is consistent with lora-peer and
// landlink-device, which now identify peers numerically.
const keys = new Map<number, Uint8Array>();
const listeners = new Set<() => void>();
let snapshot: ReadonlyMap<number, Uint8Array> = new Map();
let saveTimer: ReturnType<typeof setTimeout> | null = null;

function hasStorage(): boolean {
  try {
    return typeof globalThis.localStorage !== "undefined";
  } catch {
    return false;
  }
}

function hexToBytes(hex: string): Uint8Array | null {
  if (hex.length !== PUBLIC_KEY_BYTES * 2) return null;
  const out = new Uint8Array(PUBLIC_KEY_BYTES);
  for (let i = 0; i < PUBLIC_KEY_BYTES; i++) {
    const c = hex.slice(i * 2, i * 2 + 2);
    const v = Number.parseInt(c, 16);
    if (!Number.isFinite(v)) return null;
    out[i] = v & 0xff;
  }
  return out;
}

function bytesToHex(bytes: Uint8Array): string {
  let out = "";
  for (let i = 0; i < bytes.byteLength; i++) {
    out += (bytes[i] ?? 0).toString(16).padStart(2, "0");
  }
  return out;
}

function loadFromStorage(): void {
  if (!hasStorage()) return;
  try {
    const raw = globalThis.localStorage.getItem(STORAGE_KEY);
    if (!raw) return;
    const parsed: unknown = JSON.parse(raw);
    if (parsed === null || typeof parsed !== "object") return;
    for (const [nodeNumStr, hex] of Object.entries(
      parsed as Record<string, unknown>,
    )) {
      if (typeof hex !== "string") continue;
      const nodeNum = Number.parseInt(nodeNumStr, 10);
      if (!Number.isFinite(nodeNum)) continue;
      const bytes = hexToBytes(hex);
      if (!bytes) continue;
      keys.set(nodeNum, bytes);
    }
    rebuildSnapshot();
  } catch (err) {
    console.warn("[meshtastic-pki] load failed", err);
  }
}

function persist(): void {
  if (!hasStorage()) return;
  const obj: Record<string, string> = {};
  for (const [nodeNum, key] of keys) {
    obj[nodeNum.toString(10)] = bytesToHex(key);
  }
  try {
    globalThis.localStorage.setItem(STORAGE_KEY, JSON.stringify(obj));
  } catch (err) {
    console.warn("[meshtastic-pki] persist failed", err);
  }
}

function schedulePersist(): void {
  if (saveTimer !== null) clearTimeout(saveTimer);
  saveTimer = setTimeout(() => {
    saveTimer = null;
    persist();
  }, SAVE_DEBOUNCE_MS);
}

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
  schedulePersist();
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
  if (saveTimer !== null) {
    clearTimeout(saveTimer);
    saveTimer = null;
  }
  if (hasStorage()) {
    try {
      globalThis.localStorage.removeItem(STORAGE_KEY);
    } catch {
      // best effort
    }
  }
}

function bytesEqual(a: Uint8Array, b: Uint8Array): boolean {
  if (a.byteLength !== b.byteLength) return false;
  for (let i = 0; i < a.byteLength; i++) {
    if (a[i] !== b[i]) return false;
  }
  return true;
}

loadFromStorage();

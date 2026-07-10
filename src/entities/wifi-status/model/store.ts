// Persistent per-device Wi-Fi status. The device firmware maintains its Wi-Fi
// connection and reports WIFI_STATUS; the client caches the latest here (keyed
// by device id) and persists to localStorage, so the "connected to Wi-Fi" state
// survives a BLE reconnect or a page reload. Mirrors the hand-rolled
// external-store pattern used across the app.

import type { WifiDeviceStatus } from "./types";

const STORAGE_KEY = "vision.wifi-status.v1";
const SAVE_DEBOUNCE_MS = 300;

const statuses = new Map<string, WifiDeviceStatus>();
const listeners = new Set<() => void>();
let snapshot: ReadonlyMap<string, WifiDeviceStatus> = new Map();
let saveTimer: ReturnType<typeof setTimeout> | null = null;

function hasStorage(): boolean {
  try {
    return typeof globalThis.localStorage !== "undefined";
  } catch {
    return false;
  }
}

function rebuild(): void {
  snapshot = new Map(statuses);
}

function emit(): void {
  rebuild();
  for (const cb of listeners) {
    try {
      cb();
    } catch {
      // listeners must not break each other
    }
  }
}

function persist(): void {
  if (!hasStorage()) return;
  const obj: Record<string, WifiDeviceStatus> = {};
  for (const [id, status] of statuses) obj[id] = status;
  try {
    globalThis.localStorage.setItem(STORAGE_KEY, JSON.stringify(obj));
  } catch (err) {
    console.warn("[wifi-status] persist failed", err);
  }
}

function schedulePersist(): void {
  if (saveTimer !== null) clearTimeout(saveTimer);
  saveTimer = setTimeout(() => {
    saveTimer = null;
    persist();
  }, SAVE_DEBOUNCE_MS);
}

function loadFromStorage(): void {
  if (!hasStorage()) return;
  try {
    const raw = globalThis.localStorage.getItem(STORAGE_KEY);
    if (!raw) return;
    const parsed: unknown = JSON.parse(raw);
    if (parsed === null || typeof parsed !== "object") return;
    for (const [id, value] of Object.entries(parsed as Record<string, unknown>)) {
      if (value === null || typeof value !== "object") continue;
      const v = value as Record<string, unknown>;
      if (typeof v["connected"] !== "boolean") continue;
      statuses.set(id, {
        connected: v["connected"],
        ip: typeof v["ip"] === "string" ? v["ip"] : null,
        updatedAt: typeof v["updatedAt"] === "number" ? v["updatedAt"] : 0,
      });
    }
    rebuild();
  } catch (err) {
    console.warn("[wifi-status] load failed", err);
  }
}

export function recordWifiStatus(
  deviceId: string,
  input: { connected: boolean; ip: string | null },
): void {
  const current = statuses.get(deviceId);
  // Ignore no-op updates so useSyncExternalStore keeps a stable snapshot.
  if (current?.connected === input.connected && current?.ip === input.ip) {
    return;
  }
  statuses.set(deviceId, {
    connected: input.connected,
    ip: input.ip,
    updatedAt: Date.now(),
  });
  emit();
  schedulePersist();
}

export function getWifiStatus(deviceId: string | null): WifiDeviceStatus | null {
  if (!deviceId) return null;
  return snapshot.get(deviceId) ?? null;
}

export function subscribeWifiStatus(cb: () => void): () => void {
  listeners.add(cb);
  return () => {
    listeners.delete(cb);
  };
}

export function clearWifiStatus(deviceId: string): void {
  if (statuses.delete(deviceId)) {
    emit();
    schedulePersist();
  }
}

export function _resetWifiStatusStore(): void {
  statuses.clear();
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

loadFromStorage();

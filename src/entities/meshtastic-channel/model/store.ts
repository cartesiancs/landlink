import { pskFromBase64, pskToBase64 } from "../lib/encode-psk";
import { MAX_CHANNEL_INDEX, type Channel } from "./types";

// Per-device channel map. Keys are BLE device ids; values are the list of
// SECONDARY channels (1..7) the user has created. Primary (index 0) is
// always synthesized at read time so it never depends on persistence.

const STORAGE_KEY = "landlink.meshtastic-channels.v1";

type SerializedChannel = {
  index: number;
  name: string;
  psk: string;
  role: "primary" | "secondary";
  createdAt: number;
};

type SerializedMap = Record<string, SerializedChannel[]>;

let secondariesByDevice = new Map<string, Channel[]>();
// Device-synced channel lists (Meshtastic adapter pushes the full set,
// including Primary, after the want_config_id flow). Kept in-memory only —
// the device is the source of truth, so we re-fetch on reconnect rather
// than risk stale persistence.
const deviceSyncedByDevice = new Map<string, Channel[]>();
const listeners = new Set<() => void>();

// Stable reference for "no secondary channels" so useSyncExternalStore
// snapshot reads return Object.is-equal values across renders. Without this
// the `?? []` fallback in getSecondaries would mint a new array on every
// call and trigger React's infinite re-render guard.
const EMPTY_SECONDARIES: readonly Channel[] = Object.freeze([]);

function emit(): void {
  for (const l of listeners) l();
}

function persist(): void {
  const out: SerializedMap = {};
  for (const [deviceId, channels] of secondariesByDevice) {
    out[deviceId] = channels.map((c) => ({
      index: c.index,
      name: c.name,
      psk: pskToBase64(c.psk),
      role: c.role,
      createdAt: c.createdAt,
    }));
  }
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(out));
  } catch {
    // localStorage may be full or disabled in privacy modes; we accept the
    // loss rather than crashing the chat UI.
  }
}

function hydrate(): void {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return;
    const parsed = JSON.parse(raw) as SerializedMap;
    const next = new Map<string, Channel[]>();
    for (const [deviceId, channels] of Object.entries(parsed)) {
      next.set(
        deviceId,
        channels.map((c) => ({
          index: c.index,
          name: c.name,
          psk: pskFromBase64(c.psk),
          role: c.role,
          createdAt: c.createdAt,
        })),
      );
    }
    secondariesByDevice = next;
  } catch {
    secondariesByDevice = new Map();
  }
}

hydrate();

export function subscribe(l: () => void): () => void {
  listeners.add(l);
  return () => {
    listeners.delete(l);
  };
}

export function getSecondaries(deviceId: string): readonly Channel[] {
  return secondariesByDevice.get(deviceId) ?? EMPTY_SECONDARIES;
}

// Returns the smallest available secondary index (1..7), or null when full.
export function nextFreeIndex(deviceId: string): number | null {
  const used = new Set(getSecondaries(deviceId).map((c) => c.index));
  for (let i = 1; i <= MAX_CHANNEL_INDEX; i++) {
    if (!used.has(i)) return i;
  }
  return null;
}

export function addSecondary(deviceId: string, channel: Channel): void {
  if (channel.role !== "secondary" || channel.index === 0) return;
  const existing = secondariesByDevice.get(deviceId) ?? [];
  if (existing.some((c) => c.index === channel.index)) return;
  const next = [...existing, channel].sort((a, b) => a.index - b.index);
  secondariesByDevice.set(deviceId, next);
  persist();
  emit();
}

export function removeSecondary(deviceId: string, index: number): void {
  if (index === 0) return;
  const existing = secondariesByDevice.get(deviceId);
  if (!existing) return;
  const next = existing.filter((c) => c.index !== index);
  if (next.length === existing.length) return;
  if (next.length === 0) {
    secondariesByDevice.delete(deviceId);
  } else {
    secondariesByDevice.set(deviceId, next);
  }
  persist();
  emit();
}

// Device-synced API ---------------------------------------------------------
// Used by the Meshtastic adapter to push the full channel set (including
// Primary at index 0) from the connected device's FromRadio.channel stream.

export function setDeviceChannels(
  deviceId: string,
  channels: readonly Channel[],
): void {
  const sorted = [...channels].sort((a, b) => a.index - b.index);
  deviceSyncedByDevice.set(deviceId, sorted);
  emit();
}

export function clearDeviceChannels(deviceId: string): void {
  if (!deviceSyncedByDevice.has(deviceId)) return;
  deviceSyncedByDevice.delete(deviceId);
  emit();
}

export function getDeviceChannels(
  deviceId: string,
): readonly Channel[] | null {
  return deviceSyncedByDevice.get(deviceId) ?? null;
}

// Single source of truth for per-device channel lists. The connected device
// (Landlink-protocol or Meshtastic-protocol — they share a single registry
// at the firmware level now) is authoritative. localStorage is only a cache
// keyed by deviceId so the channels render instantly on app open before the
// BLE sync round-trips. Any writes flow through setChannels(), which the
// device sync feature drives in response to CHANNEL_LIST_RESULT /
// CHANNEL_RESULT EVTs.

import { pskFromBase64, pskToBase64 } from "../lib/encode-psk";
import { MAX_CHANNEL_INDEX, type Channel } from "./types";

const STORAGE_KEY = "landlink.channels.v2";

type SerializedChannel = {
  index: number;
  name: string;
  psk: string;
  role: "primary" | "secondary";
  createdAt: number;
};

type SerializedMap = Record<string, SerializedChannel[]>;

let channelsByDevice = new Map<string, readonly Channel[]>();
const listeners = new Set<() => void>();

// Stable reference for "no channels yet" so useSyncExternalStore snapshots
// remain Object.is-equal across renders.
const EMPTY_CHANNELS: readonly Channel[] = Object.freeze([]);

function emit(): void {
  for (const l of listeners) l();
}

function persist(): void {
  const out: SerializedMap = {};
  for (const [deviceId, channels] of channelsByDevice) {
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
    // localStorage may be full or disabled in privacy modes; the in-memory
    // map remains correct, so we accept the cache loss silently.
  }
}

function hydrate(): void {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return;
    const parsed = JSON.parse(raw) as SerializedMap;
    const next = new Map<string, readonly Channel[]>();
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
    channelsByDevice = next;
  } catch {
    channelsByDevice = new Map();
  }
}

hydrate();

export function subscribe(l: () => void): () => void {
  listeners.add(l);
  return () => {
    listeners.delete(l);
  };
}

// Returns the cached channel list for a device, or null when we have no
// data yet (no localStorage entry and no sync completed). Callers
// distinguish "loading" (null) from "no channels" (empty array — only
// possible if the device explicitly told us so, which the firmware never
// does because Primary is mandatory).
export function getChannels(deviceId: string): readonly Channel[] | null {
  return channelsByDevice.get(deviceId) ?? null;
}

// Replace the channel list for a device. Called by the sync feature after
// a CHANNEL_LIST round-trip and after each unsolicited CHANNEL_RESULT.
export function setChannels(
  deviceId: string,
  channels: readonly Channel[],
): void {
  const sorted = [...channels].sort((a, b) => a.index - b.index);
  channelsByDevice.set(deviceId, sorted);
  persist();
  emit();
}

// Apply an upsert against the in-memory list and persist. Used by the sync
// feature when a single-slot CHANNEL_RESULT arrives — avoids a full
// CHANNEL_LIST refetch on every mutation.
export function upsertChannel(deviceId: string, channel: Channel): void {
  const existing = channelsByDevice.get(deviceId) ?? EMPTY_CHANNELS;
  const filtered = existing.filter((c) => c.index !== channel.index);
  const next = [...filtered, channel].sort((a, b) => a.index - b.index);
  channelsByDevice.set(deviceId, next);
  persist();
  emit();
}

export function removeChannel(deviceId: string, index: number): void {
  const existing = channelsByDevice.get(deviceId);
  if (!existing) return;
  const next = existing.filter((c) => c.index !== index);
  if (next.length === existing.length) return;
  channelsByDevice.set(deviceId, next);
  persist();
  emit();
}

export function clearChannels(deviceId: string): void {
  if (!channelsByDevice.has(deviceId)) return;
  channelsByDevice.delete(deviceId);
  persist();
  emit();
}

// Returns the smallest available secondary index (1..7) for a device based
// on the currently cached list, or null when all 8 slots are in use. Used
// by the create-channel UI to suggest the next slot before round-tripping
// CHANNEL_SET to the device.
export function nextFreeIndex(deviceId: string): number | null {
  const channels = channelsByDevice.get(deviceId) ?? EMPTY_CHANNELS;
  const used = new Set(channels.map((c) => c.index));
  for (let i = 1; i <= MAX_CHANNEL_INDEX; i++) {
    if (!used.has(i)) return i;
  }
  return null;
}

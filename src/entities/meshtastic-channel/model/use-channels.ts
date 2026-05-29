import { useSyncExternalStore } from "react";

import { useActiveDeviceId } from "@/entities/registered-device";

import { getChannels, subscribe } from "./store";
import type { Channel } from "./types";

// Returns the cached channel list for the active device (connected, or the
// most recently used registered device when offline), or null when no device
// is registered at all. The device sync feature is responsible for populating
// the store on connect; until that completes the UI sees either:
//   * an empty/missing cache → null (UI shows a brief loading affordance)
//   * a previously-cached snapshot from localStorage → that snapshot, then
//     the fresh device-supplied list overwrites it.
// When BLE drops, the cache for the last-connected device remains readable
// so channels stay viewable in a read-only state.
export function useChannels(): readonly Channel[] | null {
  const deviceId = useActiveDeviceId();
  return useSyncExternalStore(
    subscribe,
    () => (deviceId ? getChannels(deviceId) : null),
    () => (deviceId ? getChannels(deviceId) : null),
  );
}

export function findChannel(
  channels: readonly Channel[] | null,
  index: number,
): Channel | null {
  if (!channels) return null;
  return channels.find((c) => c.index === index) ?? null;
}

import { useSyncExternalStore } from "react";

import { useLandlinkDevice } from "@/entities/landlink-device";

import { getChannels, subscribe } from "./store";
import type { Channel } from "./types";

// Returns the cached channel list for the connected device, or null when no
// device is connected and no cached list exists. The device sync feature is
// responsible for populating the store on connect; until that completes the
// UI sees either:
//   * an empty/missing cache → null (UI shows a brief loading affordance)
//   * a previously-cached snapshot from localStorage → that snapshot, then
//     the fresh device-supplied list overwrites it.
export function useChannels(): readonly Channel[] | null {
  const device = useLandlinkDevice();
  const deviceId = device?.deviceId ?? null;
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

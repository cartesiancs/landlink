import { useMemo, useSyncExternalStore } from "react";

import { useLandlinkDevice } from "@/entities/landlink-device";

import { makePrimary } from "../lib/defaults";
import { getSecondaries, subscribe } from "./store";
import type { Channel } from "./types";

// Returns channels for the currently connected device:
// Primary (synthesized) at index 0, then any user-created secondary channels
// stored locally. Returns null when no device is connected so callers can
// distinguish "empty list" from "no context".
//
// IMPORTANT: getSnapshot must return Object.is-equal references for the same
// underlying state, or React's useSyncExternalStore detects spurious changes
// and triggers an infinite re-render. We therefore:
//   1. Subscribe to the raw secondaries array (the store already returns a
//      new reference only when the data actually changes).
//   2. Compose Primary at render time via useMemo so the outer array is
//      memoized by (deviceId, secondaries) identity.
export function useChannels(): readonly Channel[] | null {
  const device = useLandlinkDevice();
  const deviceId = device?.deviceId ?? null;
  const secondaries = useSyncExternalStore(
    subscribe,
    () => (deviceId ? getSecondaries(deviceId) : null),
    () => (deviceId ? getSecondaries(deviceId) : null),
  );
  return useMemo(() => {
    if (!deviceId || !secondaries) return null;
    return [makePrimary(), ...secondaries];
  }, [deviceId, secondaries]);
}

export function findChannel(
  channels: readonly Channel[] | null,
  index: number,
): Channel | null {
  if (!channels) return null;
  return channels.find((c) => c.index === index) ?? null;
}

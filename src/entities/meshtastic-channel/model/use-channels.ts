import { useMemo, useSyncExternalStore } from "react";

import { useActiveDeviceId } from "@/entities/registered-device";

import { makePrimary } from "../lib/defaults";
import { getChannels, subscribe } from "./store";
import type { Channel } from "./types";

// Returns the cached channel list for the active device, with Primary (index
// 0) guaranteed to be present.
//
// WHY the guarantee: Primary is mandatory firmware-side on every supported
// device, so the UI must show it unconditionally. Synthesising a placeholder
// here keeps the Channels screen non-empty during the gap between BLE attach
// and the CHANNEL_LIST round-trip, on a freshly paired device with no cache
// yet, and on devices whose firmware omits CHANNEL_NAME for Primary (a known
// Landlink-side shorthand that would otherwise drop the slot in parseChannel).
// The device-supplied Primary overwrites the placeholder on the next sync.
//
// Null is reserved for "no device registered at all" so the channel-list
// widget can still surface its "Pair a device" CTA.
export function useChannels(): readonly Channel[] | null {
  const deviceId = useActiveDeviceId();
  const raw = useSyncExternalStore(
    subscribe,
    () => (deviceId ? getChannels(deviceId) : null),
    () => (deviceId ? getChannels(deviceId) : null),
  );
  return useMemo(() => {
    if (deviceId === null) return null;
    if (raw === null) return [makePrimary()];
    if (raw.some((c) => c.index === 0)) return raw;
    return [makePrimary(), ...raw];
  }, [deviceId, raw]);
}

export function findChannel(
  channels: readonly Channel[] | null,
  index: number,
): Channel | null {
  if (!channels) return null;
  return channels.find((c) => c.index === index) ?? null;
}

import { useMemo, useSyncExternalStore } from "react";

import { useLandlinkDevice } from "@/entities/landlink-device";

import { makePrimary } from "../lib/defaults";
import { getDeviceChannels, getSecondaries, subscribe } from "./store";
import type { Channel } from "./types";

type Snapshot =
  | { kind: "synth"; secondaries: readonly Channel[] }
  | { kind: "device"; channels: readonly Channel[] }
  | { kind: "no-device" };

const NO_DEVICE: Snapshot = { kind: "no-device" };

// Snapshot cache so useSyncExternalStore returns Object.is-equal references
// across renders. Without caching, every call to getSnapshot would mint a
// new object literal and trigger React's infinite re-render guard.
let lastSnapshot: Snapshot = NO_DEVICE;
let lastDeviceId: string | null = null;
let lastSecondaries: readonly Channel[] | null = null;
let lastDeviceChannels: readonly Channel[] | null = null;

function getSnapshot(deviceId: string | null): Snapshot {
  if (!deviceId) {
    if (lastSnapshot.kind !== "no-device") {
      lastSnapshot = NO_DEVICE;
      lastDeviceId = null;
      lastSecondaries = null;
      lastDeviceChannels = null;
    }
    return lastSnapshot;
  }
  const deviceChannels = getDeviceChannels(deviceId);
  if (deviceChannels) {
    if (
      lastSnapshot.kind === "device" &&
      lastDeviceId === deviceId &&
      lastDeviceChannels === deviceChannels
    ) {
      return lastSnapshot;
    }
    lastSnapshot = { kind: "device", channels: deviceChannels };
    lastDeviceId = deviceId;
    lastDeviceChannels = deviceChannels;
    lastSecondaries = null;
    return lastSnapshot;
  }
  const secondaries = getSecondaries(deviceId);
  if (
    lastSnapshot.kind === "synth" &&
    lastDeviceId === deviceId &&
    lastSecondaries === secondaries
  ) {
    return lastSnapshot;
  }
  lastSnapshot = { kind: "synth", secondaries };
  lastDeviceId = deviceId;
  lastSecondaries = secondaries;
  lastDeviceChannels = null;
  return lastSnapshot;
}

// Returns channels for the currently connected device:
//   • Meshtastic device → exact channel list pushed by the device
//     (Primary at index 0 + any configured secondaries).
//   • Landlink (or no device-supplied data) → synthesized Primary +
//     user-created secondaries from localStorage.
// Returns null when no device is connected.
export function useChannels(): readonly Channel[] | null {
  const device = useLandlinkDevice();
  const deviceId = device?.deviceId ?? null;
  const snapshot = useSyncExternalStore(
    subscribe,
    () => getSnapshot(deviceId),
    () => getSnapshot(deviceId),
  );
  return useMemo(() => {
    if (snapshot.kind === "no-device") return null;
    if (snapshot.kind === "device") return snapshot.channels;
    return [makePrimary(), ...snapshot.secondaries];
  }, [snapshot]);
}

export function findChannel(
  channels: readonly Channel[] | null,
  index: number,
): Channel | null {
  if (!channels) return null;
  return channels.find((c) => c.index === index) ?? null;
}

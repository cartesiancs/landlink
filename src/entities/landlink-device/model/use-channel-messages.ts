import { useEffect, useState } from "react";

import { loadMessages } from "../api/message-store";
import { replaceChannelMessages, type MeshMessage } from "./store";
import { useLandlinkDevice } from "./use-landlink-device";

const EMPTY_MESSAGES: readonly MeshMessage[] = Object.freeze([]);

type LoadedSnapshot = {
  key: string;
  messages: readonly MeshMessage[];
};

function keyFor(deviceId: string, channelIndex: number): string {
  return `${deviceId}:${channelIndex.toString()}`;
}

// Resolves the message list for a specific (deviceId, channelIndex) pair.
//
// Two sources need to be reconciled:
//   * Live in-memory store — the connected device's `messages` array, updated
//     in real time by the BLE adapter. Source of truth while connected.
//   * IndexedDB snapshot — per-device persisted history. Loaded on mount so
//     channels remain readable after disconnect or across reloads.
//
// When the requested deviceId matches the connected device we hydrate the
// in-memory store from disk (so ACK updates apply to historical entries) and
// return its live filtered slice. Otherwise we return the disk snapshot
// directly — that path is what makes channels readable while offline. Stale
// snapshots from a prior (deviceId, channelIndex) are filtered out by a
// keyed render check so switching active devices does not flash old history.
export function useChannelMessages(
  deviceId: string | null,
  channelIndex: number,
): readonly MeshMessage[] {
  const device = useLandlinkDevice();
  const isLive = deviceId !== null && device?.deviceId === deviceId;
  const [loaded, setLoaded] = useState<LoadedSnapshot | null>(null);

  useEffect(() => {
    if (deviceId === null || channelIndex < 0) return;
    const key = keyFor(deviceId, channelIndex);
    let cancelled = false;
    void loadMessages(deviceId, channelIndex).then((msgs) => {
      if (cancelled) return;
      setLoaded({ key, messages: msgs });
      if (isLive) {
        replaceChannelMessages(channelIndex, msgs);
      }
    });
    return () => {
      cancelled = true;
    };
  }, [deviceId, channelIndex, isLive]);

  if (isLive && device) {
    return device.messages.filter(
      (m) => (m.channelIndex ?? 0) === channelIndex,
    );
  }
  if (deviceId === null || channelIndex < 0) return EMPTY_MESSAGES;
  const expected = keyFor(deviceId, channelIndex);
  return loaded?.key === expected ? loaded.messages : EMPTY_MESSAGES;
}

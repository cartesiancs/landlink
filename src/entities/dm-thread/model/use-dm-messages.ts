import { useEffect, useMemo, useState } from "react";

import {
  loadDmMessages,
  useLandlinkDevice,
  type MeshMessage,
} from "@/entities/landlink-device";

const EMPTY: readonly MeshMessage[] = Object.freeze([]);

type LoadedSnapshot = {
  key: string;
  messages: readonly MeshMessage[];
};

function keyFor(deviceId: string, peerNodeNum: number): string {
  return `${deviceId}:${peerNodeNum.toString()}`;
}

export function useDmMessages(
  peerNodeNum: number | null,
): readonly MeshMessage[] {
  const device = useLandlinkDevice();
  const deviceId = device?.deviceId ?? null;
  const selfNodeNum = device?.info?.nodeNum ?? 0;
  const [loaded, setLoaded] = useState<LoadedSnapshot | null>(null);

  useEffect(() => {
    if (deviceId === null || peerNodeNum === null) return;
    const key = keyFor(deviceId, peerNodeNum);
    let cancelled = false;
    void loadDmMessages(deviceId, peerNodeNum).then((msgs) => {
      if (cancelled) return;
      setLoaded({ key, messages: msgs });
    });
    return () => {
      cancelled = true;
    };
  }, [deviceId, peerNodeNum]);

  const liveMessages: readonly MeshMessage[] = device?.messages ?? EMPTY;

  return useMemo(() => {
    if (peerNodeNum === null || deviceId === null) return EMPTY;
    const isThreadMessage = (m: MeshMessage): boolean => {
      if (m.recipientNodeNum === undefined) return false;
      if (m.direction === "outgoing") {
        return (
          m.senderNodeNum === selfNodeNum &&
          m.recipientNodeNum === peerNodeNum
        );
      }
      return (
        m.senderNodeNum === peerNodeNum &&
        m.recipientNodeNum === selfNodeNum
      );
    };
    const live = liveMessages.filter(isThreadMessage);
    const liveById = new Map<string, MeshMessage>();
    for (const m of live) liveById.set(m.id, m);
    const expected = keyFor(deviceId, peerNodeNum);
    const persisted = loaded?.key === expected ? loaded.messages : EMPTY;
    const merged: MeshMessage[] = [];
    const seen = new Set<string>();
    for (const p of persisted) {
      if (!isThreadMessage(p)) continue;
      merged.push(liveById.get(p.id) ?? p);
      seen.add(p.id);
    }
    for (const m of live) {
      if (seen.has(m.id)) continue;
      merged.push(m);
    }
    merged.sort((a, b) => a.receivedAt - b.receivedAt);
    return merged;
  }, [liveMessages, loaded, peerNodeNum, deviceId, selfNodeNum]);
}

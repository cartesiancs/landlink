import { useEffect, useMemo, useState } from "react";

import {
  loadDmMessages,
  loadKnownDmPeers,
  useLandlinkDevice,
  type MeshMessage,
} from "@/entities/landlink-device";
import { nodeNumToHex } from "@/shared/lib";

import { deriveDmThreads } from "./derive-threads";
import type { DmThread } from "./types";

const EMPTY_THREADS: readonly DmThread[] = Object.freeze([]);
const EMPTY_MESSAGES: readonly MeshMessage[] = Object.freeze([]);

type ColdSeed = {
  peerNodeNum: number;
  lastReceivedAt: number;
  lastTextPreview: string;
};

type ColdSnapshot = {
  deviceId: string;
  seeds: readonly ColdSeed[];
};

export function useDmThreads(): readonly DmThread[] {
  const device = useLandlinkDevice();
  const deviceId = device?.deviceId ?? null;
  const selfNodeNum = device?.info?.nodeNum ?? 0;
  const [cold, setCold] = useState<ColdSnapshot | null>(null);

  useEffect(() => {
    if (!deviceId) return;
    let cancelled = false;
    void (async () => {
      const peers = await loadKnownDmPeers(deviceId);
      if (cancelled) return;
      const seeds: ColdSeed[] = [];
      for (const p of peers) {
        const msgs = await loadDmMessages(deviceId, p.peerNodeNum);
        if (cancelled) return;
        const last = msgs[msgs.length - 1];
        seeds.push({
          peerNodeNum: p.peerNodeNum,
          lastReceivedAt: p.lastReceivedAt,
          lastTextPreview: last?.text ?? "",
        });
      }
      if (!cancelled) setCold({ deviceId, seeds });
    })();
    return () => {
      cancelled = true;
    };
  }, [deviceId]);

  const liveMessages: readonly MeshMessage[] = device?.messages ?? EMPTY_MESSAGES;

  return useMemo(() => {
    if (deviceId === null) return EMPTY_THREADS;
    const live = deriveDmThreads(liveMessages, selfNodeNum);
    const seeds = cold?.deviceId === deviceId ? cold.seeds : [];
    if (seeds.length === 0) return live;
    const byPeer = new Map<number, DmThread>();
    for (const t of live) byPeer.set(t.peerNodeNum, t);
    for (const seed of seeds) {
      if (byPeer.has(seed.peerNodeNum)) continue;
      byPeer.set(seed.peerNodeNum, {
        peerNodeNum: seed.peerNodeNum,
        peerNodeIdHex: nodeNumToHex(seed.peerNodeNum),
        lastReceivedAt: seed.lastReceivedAt,
        lastTextPreview: seed.lastTextPreview,
        unreadCount: 0,
      });
    }
    const out: DmThread[] = [];
    for (const t of byPeer.values()) out.push(t);
    out.sort((a, b) => b.lastReceivedAt - a.lastReceivedAt);
    return out;
  }, [liveMessages, selfNodeNum, cold, deviceId]);
}

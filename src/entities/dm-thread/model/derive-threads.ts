import { type MeshMessage } from "@/entities/landlink-device";
import { nodeNumToHex } from "@/shared/lib";

import type { DmThread } from "./types";

const PREVIEW_MAX_CHARS = 60;

function trimPreview(text: string): string {
  const cleaned = text.replace(/\s+/g, " ").trim();
  if (cleaned.length <= PREVIEW_MAX_CHARS) return cleaned;
  return cleaned.slice(0, PREVIEW_MAX_CHARS - 1) + "...";
}

export function deriveDmThreads(
  messages: readonly MeshMessage[],
  selfNodeNum: number,
): readonly DmThread[] {
  if (!Number.isFinite(selfNodeNum) || selfNodeNum === 0) return [];
  const byPeer = new Map<number, {
    lastReceivedAt: number;
    lastTextPreview: string;
    unreadCount: number;
  }>();
  for (const m of messages) {
    if (m.recipientNodeNum === undefined) continue;
    let peer: number;
    if (m.direction === "outgoing") {
      if (m.senderNodeNum !== selfNodeNum) continue;
      peer = m.recipientNodeNum;
    } else {
      if (m.recipientNodeNum !== selfNodeNum) continue;
      peer = m.senderNodeNum;
    }
    const prev = byPeer.get(peer);
    if (!prev || m.receivedAt > prev.lastReceivedAt) {
      byPeer.set(peer, {
        lastReceivedAt: m.receivedAt,
        lastTextPreview: trimPreview(m.text),
        unreadCount: 0,
      });
    }
  }
  const out: DmThread[] = [];
  for (const [peerNodeNum, info] of byPeer) {
    out.push({
      peerNodeNum,
      peerNodeIdHex: nodeNumToHex(peerNodeNum),
      lastReceivedAt: info.lastReceivedAt,
      lastTextPreview: info.lastTextPreview,
      unreadCount: info.unreadCount,
    });
  }
  out.sort((a, b) => b.lastReceivedAt - a.lastReceivedAt);
  return out;
}

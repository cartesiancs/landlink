import type { LoraPeer, LoraPeerSource } from "./types";

// WHY: discovery cadence is 30s; drop a peer after 3 missed cycles so a
// device that went offline disappears from the list within ~90s.
export const PEER_TTL_MS = 90_000;

// Higher rank wins on collision. A beacon update carrying fresh telemetry
// must not be downgraded by a later history hydrate; a chat-source entry
// must not be overwritten by a history backfill that has no telemetry.
const SOURCE_RANK: Record<LoraPeerSource, number> = {
  history: 0,
  chat: 1,
  beacon: 2,
};

const peers = new Map<string, LoraPeer>();
const listeners = new Set<() => void>();
let snapshot: readonly LoraPeer[] = [];

function rebuildSnapshot(): void {
  const arr: LoraPeer[] = [];
  for (const peer of peers.values()) arr.push(peer);
  arr.sort((a, b) => b.lastSeenAt - a.lastSeenAt);
  snapshot = arr;
}

function emit(): void {
  rebuildSnapshot();
  for (const cb of listeners) {
    try {
      cb();
    } catch {
      // listeners must not break each other
    }
  }
}

export function upsertLoraPeer(peer: LoraPeer): void {
  const existing = peers.get(peer.nodeId);
  if (!existing) {
    peers.set(peer.nodeId, peer);
    emit();
    return;
  }
  const incomingRank = SOURCE_RANK[peer.source];
  const existingRank = SOURCE_RANK[existing.source];
  if (incomingRank >= existingRank) {
    // Same or stronger source — replace wholesale, advancing lastSeenAt.
    peers.set(peer.nodeId, peer);
  } else {
    // Weaker source (e.g. history arriving for an already-beaconed peer):
    // keep telemetry and source intact, but bump lastSeenAt if the weaker
    // signal is actually newer so ordering reflects recent activity.
    if (peer.lastSeenAt > existing.lastSeenAt) {
      peers.set(peer.nodeId, { ...existing, lastSeenAt: peer.lastSeenAt });
    } else {
      return;
    }
  }
  emit();
}

export function getLoraPeers(): readonly LoraPeer[] {
  return snapshot;
}

export function subscribeLoraPeers(cb: () => void): () => void {
  listeners.add(cb);
  return () => {
    listeners.delete(cb);
  };
}

export function pruneExpiredPeers(now: number): void {
  let removed = false;
  for (const [nodeId, peer] of peers) {
    // Only beacon peers expire. chat/history entries are kept indefinitely
    // so a node we've talked to remains in the list even after its beacon
    // stops being heard, and offline history doesn't churn.
    if (peer.source !== "beacon") continue;
    if (now - peer.lastSeenAt > PEER_TTL_MS) {
      peers.delete(nodeId);
      removed = true;
    }
  }
  if (removed) emit();
}

export function findLoraPeer(nodeId: string | null): LoraPeer | null {
  if (!nodeId) return null;
  return peers.get(nodeId) ?? null;
}

export function _resetLoraPeersStore(): void {
  peers.clear();
  snapshot = [];
}

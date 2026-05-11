import type { LoraPeer } from "./types";

// WHY: discovery cadence is 30s; drop a peer after 3 missed cycles so a
// device that went offline disappears from the list within ~90s.
export const PEER_TTL_MS = 90_000;

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
  peers.set(peer.nodeId, peer);
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

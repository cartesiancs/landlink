import { appendPoint as idbAppendPoint } from "../api/idb";
import type { TrackPoint, TrackSource } from "./types";

// In-memory ring of the most recent points per (source, sourceId). The
// widget reads these to render current markers without an IDB round trip.
// Polyline history comes from queryPoints() directly.

type LatestKey = string; // `${source}:${sourceId}`

const latest = new Map<LatestKey, TrackPoint>();
const listeners = new Set<() => void>();
let snapshot: readonly TrackPoint[] = [];

function makeKey(source: TrackSource, sourceId: string): LatestKey {
  return `${source}:${sourceId}`;
}

function rebuildSnapshot(): void {
  snapshot = Array.from(latest.values());
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

// Records a point. Updates the latest-per-source memory snapshot and
// persists to IndexedDB. Returns the persisted promise so callers can
// await ordering when needed (tests, prune-after-write).
export function recordPoint(point: TrackPoint): Promise<void> {
  latest.set(makeKey(point.source, point.sourceId), point);
  emit();
  return idbAppendPoint(point).catch(() => {
    // IDB failure is non-fatal; the in-memory snapshot already reflects
    // the new position. Logging here would spam during private-browsing
    // sessions where IDB is unavailable.
  });
}

export function getLatestPoints(): readonly TrackPoint[] {
  return snapshot;
}

export function getLatestPoint(
  source: TrackSource,
  sourceId: string,
): TrackPoint | null {
  return latest.get(makeKey(source, sourceId)) ?? null;
}

export function subscribeLatestPoints(cb: () => void): () => void {
  listeners.add(cb);
  return () => {
    listeners.delete(cb);
  };
}

export function _resetPositionTrackStore(): void {
  latest.clear();
  snapshot = [];
}

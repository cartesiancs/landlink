import { recordPoint, type TrackPoint, type TrackSource } from "@/entities/position-track";

import { distanceMetersE7 } from "../lib/haversine";

// Distance threshold below which a new sample is treated as GPS jitter and
// dropped. 10 m matches typical smartphone GPS accuracy in good conditions
// while still catching realistic walking pace.
const RECORD_MIN_DISTANCE_M = 10;

// Heartbeat cadence: even while stationary, record one point per minute so
// the track has a visible time axis on the map without bloating IDB.
const RECORD_MAX_INTERVAL_MS = 60_000;

type Key = string;
const lastRecorded = new Map<Key, TrackPoint>();

function keyOf(source: TrackSource, sourceId: string): Key {
  return `${source}:${sourceId}`;
}

// Push a candidate sample through the throttle. Returns whether it was
// actually persisted, useful for telemetry/tests.
export function pushSample(point: TrackPoint): boolean {
  const k = keyOf(point.source, point.sourceId);
  const prev = lastRecorded.get(k);
  if (!prev) {
    lastRecorded.set(k, point);
    void recordPoint(point);
    return true;
  }
  const elapsed = point.recordedAt - prev.recordedAt;
  const movedM = distanceMetersE7(
    prev.latE7,
    prev.lonE7,
    point.latE7,
    point.lonE7,
  );
  if (movedM >= RECORD_MIN_DISTANCE_M || elapsed >= RECORD_MAX_INTERVAL_MS) {
    lastRecorded.set(k, point);
    void recordPoint(point);
    return true;
  }
  return false;
}

export function _resetRecorder(): void {
  lastRecorded.clear();
}

import { useEffect } from "react";

import { pruneOlderThan } from "@/entities/position-track";

import { useDevicePositionMirror } from "./use-device-position";
import { usePeerPositionsMirror } from "./use-peer-positions";
import { usePhonePositionWatch } from "./use-phone-position";

// Retention: 7 days of track history per CLAUDE.md plan. Anything older
// is pruned on each app boot; further prunes are not scheduled because
// the typical session is short enough that boot-time pruning suffices.
const RETENTION_MS = 7 * 24 * 60 * 60 * 1000;

// Single entry point that wires all three GPS sources into the recorder.
// Intended to be mounted exactly once at the app shell layer.
export function useTrackRecorder(): void {
  usePhonePositionWatch();
  useDevicePositionMirror();
  usePeerPositionsMirror();

  useEffect(() => {
    const cutoff = Date.now() - RETENTION_MS;
    void pruneOlderThan(cutoff);
  }, []);
}

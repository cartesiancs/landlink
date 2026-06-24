import { useEffect, useState } from "react";

import {
  queryPoints,
  type TrackPoint,
  type TrackSource,
} from "@/entities/position-track";

const HISTORY_WINDOW_MS = 7 * 24 * 60 * 60 * 1000;

// Loads recent track points for a given (source, sourceId). Used to draw
// polyline history when the map mounts. Re-runs when sourceId changes
// (e.g. the connected device changes).
export function useTrackHistory(
  source: TrackSource,
  sourceId: string | null,
): readonly TrackPoint[] {
  const [points, setPoints] = useState<readonly TrackPoint[]>([]);
  useEffect(() => {
    if (!sourceId) return;
    let cancelled = false;
    const sinceMs = Date.now() - HISTORY_WINDOW_MS;
    queryPoints({ source, sourceId, sinceMs })
      .then((rows) => {
        if (cancelled) return;
        rows.sort((a, b) => a.recordedAt - b.recordedAt);
        setPoints(rows);
      })
      .catch(() => {
        if (cancelled) return;
        setPoints([]);
      });
    return () => {
      cancelled = true;
    };
  }, [source, sourceId]);
  // When sourceId is null we want an empty polyline. Deriving here instead
  // of clearing inside the effect avoids react-hooks/set-state-in-effect.
  return sourceId ? points : EMPTY;
}

const EMPTY: readonly TrackPoint[] = [];

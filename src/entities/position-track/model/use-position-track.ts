import { useSyncExternalStore } from "react";

import {
  getLatestPoints,
  subscribeLatestPoints,
} from "./store";
import type { TrackPoint } from "./types";

export function useLatestTrackPoints(): readonly TrackPoint[] {
  return useSyncExternalStore(
    subscribeLatestPoints,
    getLatestPoints,
    getLatestPoints,
  );
}

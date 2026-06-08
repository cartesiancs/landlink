import { useSyncExternalStore } from "react";

import {
  findLoraPeer,
  getLoraPeers,
  subscribeLoraPeers,
} from "./store";
import type { LoraPeer } from "./types";

export function useLoraPeers(): readonly LoraPeer[] {
  return useSyncExternalStore(subscribeLoraPeers, getLoraPeers, getLoraPeers);
}

export function useLoraPeer(nodeNum: number | null): LoraPeer | null {
  return useSyncExternalStore(
    subscribeLoraPeers,
    () => findLoraPeer(nodeNum),
    () => findLoraPeer(nodeNum),
  );
}

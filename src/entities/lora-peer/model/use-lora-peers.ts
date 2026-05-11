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

export function useLoraPeer(nodeId: string | null): LoraPeer | null {
  return useSyncExternalStore(
    subscribeLoraPeers,
    () => findLoraPeer(nodeId),
    () => findLoraPeer(nodeId),
  );
}

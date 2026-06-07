import { useSyncExternalStore } from "react";

import {
  findPublicKey,
  getPublicKeys,
  subscribePublicKeys,
} from "./store";

export function usePublicKey(nodeId: string | null): Uint8Array | null {
  return useSyncExternalStore(
    subscribePublicKeys,
    () => findPublicKey(nodeId),
    () => findPublicKey(nodeId),
  );
}

export function usePublicKeys(): ReadonlyMap<string, Uint8Array> {
  return useSyncExternalStore(subscribePublicKeys, getPublicKeys, getPublicKeys);
}

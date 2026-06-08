import { useSyncExternalStore } from "react";

import {
  findPublicKey,
  getPublicKeys,
  subscribePublicKeys,
} from "./store";

export function usePublicKey(nodeNum: number | null): Uint8Array | null {
  return useSyncExternalStore(
    subscribePublicKeys,
    () => findPublicKey(nodeNum),
    () => findPublicKey(nodeNum),
  );
}

export function usePublicKeys(): ReadonlyMap<number, Uint8Array> {
  return useSyncExternalStore(subscribePublicKeys, getPublicKeys, getPublicKeys);
}

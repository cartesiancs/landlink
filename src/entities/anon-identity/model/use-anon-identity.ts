import { useEffect, useSyncExternalStore } from "react";

import {
  getAnonIdentitySnapshot,
  loadAnonIdentity,
  subscribeAnonIdentity,
} from "./store";
import type { AnonIdentity } from "./types";

// Subscribe to the anonymous identity and trigger a lazy IndexedDB load on
// mount. Returns null until the load resolves (or if none has been created).
export function useAnonIdentity(): AnonIdentity | null {
  const identity = useSyncExternalStore(
    subscribeAnonIdentity,
    getAnonIdentitySnapshot,
    getAnonIdentitySnapshot,
  );

  useEffect(() => {
    void loadAnonIdentity();
  }, []);

  return identity;
}

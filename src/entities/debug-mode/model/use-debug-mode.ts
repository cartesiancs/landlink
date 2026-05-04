import { useSyncExternalStore } from "react";

import { getDebugMode, subscribeDebugMode } from "./store";

export function useDebugMode(): boolean {
  return useSyncExternalStore(
    subscribeDebugMode,
    getDebugMode,
    getDebugMode,
  );
}

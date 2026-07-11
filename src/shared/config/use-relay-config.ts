import { useSyncExternalStore } from "react";

import { getRelayConfig, subscribeRelayConfig, type RelayConfig } from "./relay";

// Reactive view of the runtime relay config, so UI (settings form, enroll
// gating) re-renders when the user enables/disables relay or edits the URL.
export function useRelayConfig(): RelayConfig {
  return useSyncExternalStore(
    subscribeRelayConfig,
    getRelayConfig,
    getRelayConfig,
  );
}

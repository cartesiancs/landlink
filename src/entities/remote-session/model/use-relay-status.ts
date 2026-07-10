import { useSyncExternalStore } from "react";

import { getRelayState, subscribeRelayState, type RelayState } from "./store";

export function useRelayStatus(): RelayState {
  return useSyncExternalStore(subscribeRelayState, getRelayState, getRelayState);
}

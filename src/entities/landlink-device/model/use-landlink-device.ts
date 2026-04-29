import { useSyncExternalStore } from "react";

import { getState, subscribe, type LandlinkDevice } from "./store";

export function useLandlinkDevice(): LandlinkDevice | null {
  return useSyncExternalStore(subscribe, getState, getState);
}

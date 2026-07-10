import { useSyncExternalStore } from "react";

import { getWifiStatus, subscribeWifiStatus } from "./store";
import type { WifiDeviceStatus } from "./types";

export function useWifiStatus(deviceId: string | null): WifiDeviceStatus | null {
  return useSyncExternalStore(
    subscribeWifiStatus,
    () => getWifiStatus(deviceId),
    () => getWifiStatus(deviceId),
  );
}

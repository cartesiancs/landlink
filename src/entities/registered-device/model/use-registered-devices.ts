import { useSyncExternalStore } from "react";

import {
  getRegisteredDevices,
  subscribeRegisteredDevices,
} from "./store";
import type { RegisteredDevice } from "./types";

export function useRegisteredDevices(): readonly RegisteredDevice[] {
  return useSyncExternalStore(
    subscribeRegisteredDevices,
    getRegisteredDevices,
    getRegisteredDevices,
  );
}

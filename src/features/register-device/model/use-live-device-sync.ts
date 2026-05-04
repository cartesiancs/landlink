import { useEffect, useRef } from "react";

import { useLandlinkDevice } from "@/entities/landlink-device";
import {
  findDevice,
  getRegisteredDevices,
  updateRegisteredDevice,
} from "@/entities/registered-device";

export function useLiveDeviceSync(): void {
  const live = useLandlinkDevice();
  const previousIdRef = useRef<string | null>(null);

  useEffect(() => {
    const previousId = previousIdRef.current;

    if (live === null) {
      if (previousId !== null) {
        const registered = findDevice(getRegisteredDevices(), previousId);
        if (registered) {
          updateRegisteredDevice(previousId, { status: "disconnected" });
        }
        previousIdRef.current = null;
      }
      return;
    }

    if (previousId !== null && previousId !== live.deviceId) {
      const prev = findDevice(getRegisteredDevices(), previousId);
      if (prev) {
        updateRegisteredDevice(previousId, { status: "disconnected" });
      }
    }

    previousIdRef.current = live.deviceId;

    const registered = findDevice(getRegisteredDevices(), live.deviceId);
    if (!registered) return;

    if (live.status === "connected") {
      updateRegisteredDevice(live.deviceId, {
        status: "connected",
        lastConnectedAt: Date.now(),
      });
    } else if (live.status === "disconnected") {
      updateRegisteredDevice(live.deviceId, { status: "disconnected" });
    }
  }, [live]);
}

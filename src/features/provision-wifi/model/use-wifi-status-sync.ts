import { useEffect } from "react";

import { getState, onLandlinkEvt } from "@/entities/landlink-device";
import { recordWifiStatus } from "@/entities/wifi-status";
import { Opcode } from "@/shared/protocol";

import { parseWifiStatus, WifiState } from "../lib/parse-wifi";

// App-wide bridge: record every WIFI_STATUS event (solicited via WIFI_GET_STATUS
// on connect, or unsolicited on state change) into the persistent wifi-status
// store, keyed by the active device id. This is what keeps "connected to Wi-Fi"
// visible across BLE reconnects.
export function useWifiStatusSync(): void {
  useEffect(() => {
    const unsubscribe = onLandlinkEvt((frame) => {
      if (frame.opcode !== Opcode.WIFI_STATUS) return;
      const status = parseWifiStatus(frame.payload);
      if (!status) return;
      const deviceId = getState()?.deviceId;
      if (!deviceId) return;
      recordWifiStatus(deviceId, {
        connected: status.state === WifiState.CONNECTED,
        ip: status.ip,
      });
    });
    return unsubscribe;
  }, []);
}

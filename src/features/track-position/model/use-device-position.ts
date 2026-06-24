import { useEffect } from "react";

import { useLandlinkDevice } from "@/entities/landlink-device";
import type { TrackPoint } from "@/entities/position-track";

import { pushSample } from "./recorder";

// Mirrors the connected Landlink device's telemetry.gps into the recorder.
// Runs whenever the device's gps fix changes. The hook is intentionally
// passive: it does not negotiate or poll; the BLE adapter already pushes
// telemetry updates into the device store.
export function useDevicePositionMirror(): void {
  const device = useLandlinkDevice();
  const gps = device?.telemetry?.gps ?? null;
  const deviceId = device?.deviceId ?? null;

  useEffect(() => {
    if (!gps || !deviceId) return;
    const point: TrackPoint = {
      source: "device",
      sourceId: deviceId,
      latE7: gps.latE7,
      lonE7: gps.lonE7,
      altM: gps.altM,
      hdopX10: gps.hdopX10,
      speedKmhX10: gps.speedKmhX10,
      recordedAt: device?.telemetry?.receivedAt ?? Date.now(),
    };
    pushSample(point);
  }, [gps, deviceId, device?.telemetry?.receivedAt]);
}

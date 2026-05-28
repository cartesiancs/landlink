import { detectDeviceProtocol } from "./ble";
import {
  LANDLINK_SERVICE_UUID,
  MESHTASTIC_SERVICE_UUID,
} from "@/shared/protocol";

export type DeviceProtocolKind = "landlink" | "meshtastic";

// Maps a detected service UUID to its protocol family. Returns null when the
// device exposes neither (e.g. a Meshtastic device that hasn't fully booted
// or a non-supported peripheral that slipped through the pairing filter).
export async function detectDeviceProtocolKind(
  deviceId: string,
): Promise<DeviceProtocolKind | null> {
  const svc = await detectDeviceProtocol(deviceId);
  if (svc === null) return null;
  const norm = svc.toLowerCase();
  if (norm === LANDLINK_SERVICE_UUID.toLowerCase()) return "landlink";
  if (norm === MESHTASTIC_SERVICE_UUID.toLowerCase()) return "meshtastic";
  return null;
}

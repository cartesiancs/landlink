import { detachLandlinkClient } from "@/entities/landlink-device";
import { detachMeshtasticClient } from "@/entities/meshtastic-device";
import {
  findDevice,
  getPrimaryDeviceId,
  getRegisteredDevices,
  setPrimaryDeviceId,
} from "@/entities/registered-device";

// Fully disconnect a device (both transports). WHY: clear primary BEFORE
// detaching so useLiveDeviceSync's null-live branch doesn't immediately
// schedule a backoff reconnect against a device the user just chose to
// disconnect.
export async function disconnectDevice(deviceId: string): Promise<void> {
  const registered = findDevice(getRegisteredDevices(), deviceId);
  const protocol = registered?.protocol;

  if (getPrimaryDeviceId() === deviceId) {
    setPrimaryDeviceId(null);
  }

  if (protocol === "meshtastic") {
    await detachMeshtasticClient(deviceId).catch(() => undefined);
  } else {
    await detachLandlinkClient(deviceId).catch(() => undefined);
  }
}

import { detachLandlinkClient } from "@/entities/landlink-device";
import { detachMeshtasticClient } from "@/entities/meshtastic-device";
import {
  findDevice,
  getPrimaryDeviceId,
  getRegisteredDevices,
  setPrimaryDeviceId,
  updateRegisteredDevice,
} from "@/entities/registered-device";

// Fully disconnect a device (both transports). WHY: clear primary BEFORE
// detaching so useLiveDeviceSync's null-live branch doesn't immediately
// schedule a backoff reconnect against a device the user just chose to
// disconnect. Also clears the relay transport preference so a later manual
// reconnect starts from Bluetooth.
export async function disconnectDevice(deviceId: string): Promise<void> {
  const registered = findDevice(getRegisteredDevices(), deviceId);
  const protocol = registered?.protocol;

  if (getPrimaryDeviceId() === deviceId) {
    setPrimaryDeviceId(null);
  }
  if (registered?.preferRemote === true) {
    updateRegisteredDevice(deviceId, { preferRemote: false });
  }

  if (protocol === "meshtastic") {
    await detachMeshtasticClient(deviceId).catch(() => undefined);
  } else {
    await detachLandlinkClient(deviceId).catch(() => undefined);
  }
}

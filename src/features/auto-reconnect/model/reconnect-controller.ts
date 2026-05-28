import {
  attachLandlinkClient,
  detachLandlinkClient,
} from "@/entities/landlink-device";
import {
  attachMeshtasticClient,
  detachMeshtasticClient,
} from "@/entities/meshtastic-device";
import {
  findDevice,
  getRegisteredDevices,
} from "@/entities/registered-device";
import {
  detectDeviceProtocolKind,
  reconnectLandlinkDevice,
} from "@/shared/api";

type Attempt = {
  promise: Promise<void>;
};

const inFlight = new Map<string, Attempt>();

async function runAttempt(deviceId: string, name: string): Promise<void> {
  try {
    await reconnectLandlinkDevice(deviceId);
  } catch (err) {
    inFlight.delete(deviceId);
    throw err;
  }
  // Prefer the persisted protocol tag for fast-path reconnect; fall back to
  // re-probing GATT services when the tag is missing (legacy entries).
  const registered = findDevice(getRegisteredDevices(), deviceId);
  let kind = registered?.protocol ?? null;
  kind ??= (await detectDeviceProtocolKind(deviceId)) ?? "landlink";
  try {
    if (kind === "meshtastic") {
      await attachMeshtasticClient(deviceId, name);
    } else {
      await attachLandlinkClient(deviceId, name);
    }
  } catch (err) {
    if (kind === "meshtastic") {
      await detachMeshtasticClient(deviceId).catch(() => undefined);
    } else {
      await detachLandlinkClient(deviceId).catch(() => undefined);
    }
    inFlight.delete(deviceId);
    throw err;
  }
  inFlight.delete(deviceId);
}

export const reconnectController = {
  attempt(deviceId: string, name: string): Promise<void> {
    const existing = inFlight.get(deviceId);
    if (existing) return existing.promise;
    const promise = runAttempt(deviceId, name);
    inFlight.set(deviceId, { promise });
    return promise;
  },
  isAttempting(deviceId: string): boolean {
    return inFlight.has(deviceId);
  },
};

import { getAnonSigner, loadAnonIdentity } from "@/entities/anon-identity";
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
  type RegisteredDevice,
} from "@/entities/registered-device";
import {
  createRemoteTransport,
  ensureRelaySession,
} from "@/entities/remote-session";
import {
  createBleTransport,
  detectDeviceProtocolKind,
  reconnectLandlinkDevice,
} from "@/shared/api";
import { isRelayConfigured } from "@/shared/config";

type Attempt = {
  promise: Promise<void>;
};

const inFlight = new Map<string, Attempt>();

// Direct Bluetooth reconnect + attach. Throws if the device isn't reachable
// over BLE or the attach fails.
async function attachOverBle(deviceId: string, name: string): Promise<void> {
  await reconnectLandlinkDevice(deviceId);
  // Prefer the persisted protocol tag for fast-path reconnect; fall back to
  // re-probing GATT services when the tag is missing (legacy entries).
  const registered = findDevice(getRegisteredDevices(), deviceId);
  let kind = registered?.protocol ?? null;
  kind ??= (await detectDeviceProtocolKind(deviceId)) ?? "landlink";
  try {
    if (kind === "meshtastic") {
      await attachMeshtasticClient(deviceId, name);
    } else {
      await attachLandlinkClient(createBleTransport(deviceId), name);
    }
  } catch (err) {
    if (kind === "meshtastic") {
      await detachMeshtasticClient(deviceId).catch(() => undefined);
    } else {
      await detachLandlinkClient(deviceId).catch(() => undefined);
    }
    throw err;
  }
}

// Remote fallback for enrolled Landlink devices. Returns false when the device
// is not eligible (not enrolled, no relay, no identity, Meshtastic) so the
// caller can surface the original BLE error instead.
async function attachOverRemote(
  deviceId: string,
  name: string,
  registered: RegisteredDevice | null,
): Promise<boolean> {
  if (
    !registered?.remoteEnrolled ||
    !registered.rendezvousId ||
    registered.protocol === "meshtastic" ||
    !isRelayConfigured()
  ) {
    return false;
  }
  await loadAnonIdentity();
  const signer = getAnonSigner();
  if (!signer) return false;

  const session = await ensureRelaySession(signer);
  const transport = createRemoteTransport(session, deviceId, registered.rendezvousId);
  try {
    await attachLandlinkClient(transport, name);
    return true;
  } catch (err) {
    await detachLandlinkClient(deviceId).catch(() => undefined);
    throw err;
  }
}

async function runAttempt(deviceId: string, name: string): Promise<void> {
  try {
    await attachOverBle(deviceId, name);
    inFlight.delete(deviceId);
    return;
  } catch (bleErr) {
    // BLE is out of range or failed to attach. Fall back to the relay for
    // enrolled Landlink devices, so the same protocol keeps working remotely.
    const registered = findDevice(getRegisteredDevices(), deviceId);
    try {
      const ok = await attachOverRemote(deviceId, name, registered);
      if (ok) {
        inFlight.delete(deviceId);
        return;
      }
    } catch (remoteErr) {
      inFlight.delete(deviceId);
      throw remoteErr instanceof Error
        ? remoteErr
        : new Error("Remote reconnect failed.");
    }
    inFlight.delete(deviceId);
    throw bleErr instanceof Error ? bleErr : new Error("Reconnect failed.");
  }
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

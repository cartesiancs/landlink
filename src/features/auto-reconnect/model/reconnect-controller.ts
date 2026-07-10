import { Capacitor } from "@capacitor/core";

import { getAnonSigner, loadAnonIdentity } from "@/entities/anon-identity";
import {
  attachLandlinkClient,
  detachLandlinkClient,
  getState,
} from "@/entities/landlink-device";
import {
  attachMeshtasticClient,
  detachMeshtasticClient,
} from "@/entities/meshtastic-device";
import {
  findDevice,
  getRegisteredDevices,
  updateRegisteredDevice,
  type RegisteredDevice,
} from "@/entities/registered-device";
import {
  createRemoteTransport,
  ensureRelaySession,
} from "@/entities/remote-session";
import {
  createBleTransport,
  detectDeviceProtocolKind,
  isBlePairingSupported,
  listPermittedDevices,
  reconnectLandlinkDevice,
} from "@/shared/api";
import { isRelayConfigured } from "@/shared/config";

type Attempt = {
  promise: Promise<void>;
};

const inFlight = new Map<string, Attempt>();

// Whether this device can be reached over the Wi-Fi relay (enrolled Landlink
// device + relay configured). A first-class transport, not only a fallback.
export function isRemoteEligible(device: RegisteredDevice | null): boolean {
  return (
    device?.remoteEnrolled === true &&
    Boolean(device.rendezvousId) &&
    device.protocol !== "meshtastic" &&
    isRelayConfigured()
  );
}

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
  if (!isRemoteEligible(registered) || !registered?.rendezvousId) {
    return false;
  }
  try {
    await loadAnonIdentity();
    const signer = getAnonSigner();
    if (!signer) {
      console.warn("[reconnect] no anonymous identity — create an account first");
      return false;
    }
    // ensureRelaySession opens the account WebSocket to the relay. This is the
    // step that fails when the relay server is unreachable / not running.
    const session = await ensureRelaySession(signer);
    // Only proceed if the DEVICE is actually connected to the relay. The relay
    // reports this (DEVICE_ONLINE/OFFLINE); without this we'd "connect" to an
    // absent device and time out mid-attach. Returning false lets the caller
    // fall back / report cleanly.
    const online = await session.waitForDevice(registered.rendezvousId, 6000);
    if (!online) {
      console.warn(
        "[reconnect] device is not connected to the relay — check the device is flashed, on Wi-Fi, and enrolled against this relay URL",
      );
      return false;
    }
    const transport = createRemoteTransport(session, deviceId, registered.rendezvousId);
    await attachLandlinkClient(transport, name);
    return true;
  } catch (err) {
    console.warn("[reconnect] relay attach failed", err);
    await detachLandlinkClient(deviceId).catch(() => undefined);
    throw err;
  }
}

// Can we even attempt a BLE reconnect right now? Native can always try; on the
// web the device must still be in the Web Bluetooth permitted list (and the API
// must exist). When false we go straight to the relay rather than a doomed BLE
// attempt — this is what lets an enrolled device connect remotely when Bluetooth
// is off, unpermitted, or unsupported by the browser.
async function canTryBle(deviceId: string): Promise<boolean> {
  if (!isBlePairingSupported()) return false;
  if (Capacitor.isNativePlatform()) return true;
  try {
    const permitted = await listPermittedDevices();
    return permitted.some((p) => p.id === deviceId);
  } catch {
    return false;
  }
}

// Connect over the device's chosen primary transport, falling back to the other
// so the device is never stranded. Both Bluetooth and the Wi-Fi relay are
// first-class: `preferRemote` just picks which one is primary.
async function runAttempt(deviceId: string, name: string): Promise<void> {
  const registered = findDevice(getRegisteredDevices(), deviceId);

  // Each returns true on success, false if not applicable right now, throws on
  // a real connect error.
  const tryBle = async (): Promise<boolean> => {
    if (!(await canTryBle(deviceId))) return false;
    await attachOverBle(deviceId, name);
    return true;
  };
  const tryRemote = (): Promise<boolean> =>
    attachOverRemote(deviceId, name, registered);

  const order = registered?.preferRemote === true
    ? [tryRemote, tryBle]
    : [tryBle, tryRemote];

  let lastErr: unknown = null;
  for (const attach of order) {
    try {
      if (await attach()) {
        inFlight.delete(deviceId);
        return;
      }
    } catch (err) {
      lastErr = err;
    }
  }
  inFlight.delete(deviceId);
  throw lastErr instanceof Error
    ? lastErr
    : new Error("Device unreachable over Bluetooth and the Wi-Fi relay.");
}

function guardedAttempt(deviceId: string, name: string): Promise<void> {
  const existing = inFlight.get(deviceId);
  if (existing) return existing.promise;
  const promise = runAttempt(deviceId, name);
  inFlight.set(deviceId, { promise });
  return promise;
}

export const reconnectController = {
  attempt: guardedAttempt,
  isAttempting(deviceId: string): boolean {
    return inFlight.has(deviceId);
  },
  // Switch a device's active transport. mode="remote" makes the Wi-Fi relay the
  // primary; mode="ble" makes Bluetooth primary. Detaches the current link and
  // reconnects under the new preference. Returns the transport it actually
  // ended up on ("ble" | "remote"), or null if neither could be reached, so the
  // caller can report honestly (e.g. relay unreachable → fell back to BLE).
  async switchTransport(
    deviceId: string,
    name: string,
    mode: "ble" | "remote",
  ): Promise<"ble" | "remote" | null> {
    updateRegisteredDevice(deviceId, { preferRemote: mode === "remote" });
    await detachLandlinkClient(deviceId).catch(() => undefined);
    try {
      await guardedAttempt(deviceId, name);
    } catch (err) {
      console.warn("[reconnect] switchTransport failed", err);
    }
    return getState()?.transport ?? null;
  },
};

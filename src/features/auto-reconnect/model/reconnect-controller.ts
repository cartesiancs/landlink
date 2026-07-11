import { Capacitor } from "@capacitor/core";

import {
  deriveAccountSharedSecret,
  getAnonSigner,
  loadAnonIdentity,
} from "@/entities/anon-identity";
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
  type RegisteredDevice,
} from "@/entities/registered-device";
import {
  createFrameCrypto,
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
import { base64UrlToBytes } from "@/shared/lib";

type Attempt = {
  promise: Promise<void>;
};

const inFlight = new Map<string, Attempt>();

function withTimeout<T>(p: Promise<T>, ms: number): Promise<T> {
  return Promise.race([
    p,
    new Promise<T>((_, reject) =>
      setTimeout(() => reject(new Error("timed out")), ms),
    ),
  ]);
}

// Whether this device can be reached over the Wi-Fi relay (enrolled Landlink
// device + relay configured). The relay is the automatic fallback when
// Bluetooth is unavailable.
function isRemoteEligible(device: RegisteredDevice | null): boolean {
  return (
    device?.remoteEnrolled === true &&
    Boolean(device.rendezvousId) &&
    // The device ECDH key is required to derive the E2E frame key (H2); without
    // it we cannot open a secure relay link, so the device must be re-enrolled.
    Boolean(device.deviceEcdhPub) &&
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
  if (!isRemoteEligible(registered) || !registered?.rendezvousId || !registered.deviceEcdhPub) {
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
    // Derive the E2E frame key (H2) from our account ECDH key + the device's
    // ECDH key captured at enroll, so relay frames are encrypted end-to-end.
    const secret = await deriveAccountSharedSecret(
      base64UrlToBytes(registered.deviceEcdhPub),
    );
    const frameCrypto = await createFrameCrypto(secret);
    const transport = createRemoteTransport(
      session,
      deviceId,
      registered.rendezvousId,
      frameCrypto,
    );
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

// Bluetooth is always the primary transport; the Wi-Fi relay is the automatic
// fallback used only when BLE isn't reachable (off, out of range, unpermitted,
// or unsupported). The device is never stranded while either path works.
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

  let lastErr: unknown = null;
  for (const attach of [tryBle, tryRemote]) {
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
  // Bluetooth is the preferred transport. When we're currently connected over
  // the Wi-Fi relay (because BLE was unavailable when we connected), probe
  // whether BLE has become reachable again and, if so, hand the connection back
  // to Bluetooth. The probe is a real BLE connect but it does NOT disturb the
  // live relay link until it succeeds, so an out-of-range device just stays on
  // the relay. Called periodically by useLiveDeviceSync while on the relay.
  async restoreBleIfAvailable(deviceId: string, name: string): Promise<void> {
    const state = getState();
    if (
      state?.deviceId !== deviceId ||
      state.status !== "connected" ||
      state.transport !== "remote"
    ) {
      return; // not on the relay for this device
    }
    if (inFlight.has(deviceId)) return;
    if (!(await canTryBle(deviceId))) return;
    // Probe BLE reachability (bounded so an out-of-range device can't hang us).
    try {
      await withTimeout(reconnectLandlinkDevice(deviceId), 8000);
    } catch {
      return; // BLE still unreachable — stay on the relay
    }
    // Still on the relay after the probe? Then swap the live link to Bluetooth.
    const now = getState();
    if (now?.deviceId !== deviceId || now.transport !== "remote") return;
    await detachLandlinkClient(deviceId).catch(() => undefined);
    await guardedAttempt(deviceId, name).catch(() => undefined);
  },
};

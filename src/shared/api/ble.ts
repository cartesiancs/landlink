import { Capacitor } from "@capacitor/core";
import { BleClient } from "@capacitor-community/bluetooth-le";

import {
  LANDLINK_DEVICE_NAME_PREFIX,
  LANDLINK_SERVICE_UUID,
  MESHTASTIC_DEVICE_NAME_PREFIX,
  MESHTASTIC_SERVICE_UUID,
} from "@/shared/protocol";

// Devices we know how to talk to. The pairing dialog filters on these so
// either family of firmware shows up; the connect-time probe in
// detectDeviceProtocol() picks the right adapter.
const SUPPORTED_SERVICE_UUIDS: readonly string[] = [
  LANDLINK_SERVICE_UUID,
  MESHTASTIC_SERVICE_UUID,
];

export type PairedDeviceInfo = {
  id: string;
  name: string;
};

export class PairingCancelledError extends Error {
  constructor() {
    super("Pairing cancelled by user");
    this.name = "PairingCancelledError";
  }
}

// Thrown when the OS pairing flow failed or the device requires a PIN that
// the user didn't enter. Surfacing this distinct error lets the UI tell the
// user the default Meshtastic passkey is 123456.
export class PairingPinRequiredError extends Error {
  constructor() {
    super(
      "This device requires Bluetooth pairing. Enter PIN 123456 (or the code shown on the device screen) when prompted by your OS.",
    );
    this.name = "PairingPinRequiredError";
  }
}

// Heuristic match for "pairing failed because PIN wasn't entered" style
// errors. WebBluetooth and Capacitor surface different messages; we look
// for common substrings rather than parse exact error codes (which differ
// per OS).
function isLikelyPairingError(err: unknown): boolean {
  if (!(err instanceof Error)) return false;
  const m = err.message.toLowerCase();
  return (
    m.includes("authent") ||
    m.includes("insufficient") ||
    m.includes("not paired") ||
    m.includes("bonding") ||
    m.includes("pairing") ||
    m.includes("encryption") ||
    // GATT error 0x05 = insufficient authentication
    m.includes("gatt status 5") ||
    m.includes("gatt error: 5")
  );
}

export function isBlePairingSupported(): boolean {
  if (Capacitor.isNativePlatform()) return true;
  return typeof navigator !== "undefined" && Boolean(navigator.bluetooth);
}

let nativeInitPromise: Promise<void> | null = null;
function ensureNativeInitialized(): Promise<void> {
  nativeInitPromise ??= BleClient.initialize({ androidNeverForLocation: true });
  return nativeInitPromise;
}

// WHY: Web Bluetooth hands back a BluetoothDevice reference that's needed to
// call gatt.connect() later. Stash it between requestLandlinkDevice() and
// connectLandlinkDevice() so the public API can stay id-based like the native path.
const webDeviceCache = new Map<string, BluetoothDevice>();

type WebGattEntry = {
  server: BluetoothRemoteGATTServer;
  services: Map<string, BluetoothRemoteGATTService>;
  chars: Map<string, BluetoothRemoteGATTCharacteristic>;
  notifyHandlers: Map<string, (ev: Event) => void>;
  disconnectHandler: (() => void) | null;
};
const webGattCache = new Map<string, WebGattEntry>();

const disconnectListeners = new Map<string, Set<() => void>>();

function fireDisconnect(deviceId: string): void {
  const set = disconnectListeners.get(deviceId);
  if (!set) return;
  for (const cb of set) {
    try {
      cb();
    } catch {
      // listeners must not break each other
    }
  }
}

function isWebUserCancellation(err: unknown): boolean {
  return (
    err instanceof DOMException &&
    (err.name === "NotFoundError" || err.name === "AbortError")
  );
}

function isNativeUserCancellation(err: unknown): boolean {
  if (!(err instanceof Error)) return false;
  return /cancel/i.test(err.message);
}

function dvToU8(value: unknown): Uint8Array {
  if (value instanceof DataView) {
    return new Uint8Array(value.buffer, value.byteOffset, value.byteLength).slice();
  }
  if (value instanceof Uint8Array) return value.slice();
  if (value instanceof ArrayBuffer) return new Uint8Array(value).slice();
  return new Uint8Array(0);
}

function u8ToDv(value: Uint8Array): DataView {
  return new DataView(value.buffer, value.byteOffset, value.byteLength);
}

async function requestViaNative(): Promise<PairedDeviceInfo> {
  await ensureNativeInitialized();
  try {
    console.log("[ble] requestViaNative: opening native chooser");
    // Single filter listing both supported services — Capacitor BLE OR-matches
    // service UUIDs in one entry, so Meshtastic and Landlink devices both
    // appear in the system chooser.
    const device = await BleClient.requestDevice({
      services: SUPPORTED_SERVICE_UUIDS as string[],
    });
    console.log("[ble] requestViaNative: picked", {
      id: device.deviceId,
      name: device.name,
    });
    return { id: device.deviceId, name: device.name ?? "Unknown device" };
  } catch (err) {
    if (isNativeUserCancellation(err)) throw new PairingCancelledError();
    console.warn("[ble] requestViaNative failed", err);
    throw err;
  }
}

async function requestViaWeb(): Promise<PairedDeviceInfo> {
  if (typeof navigator === "undefined" || !navigator.bluetooth) {
    throw new Error("Web Bluetooth is not available in this browser.");
  }
  try {
    console.log("[ble] requestViaWeb: opening browser chooser", {
      filters: [LANDLINK_SERVICE_UUID, MESHTASTIC_SERVICE_UUID],
    });
    // Multiple filter entries → OR semantics. Devices advertising either
    // service UUID show up; the namePrefix narrows each family to its known
    // advertised name pattern to avoid showing every BLE peripheral nearby.
    const device = await navigator.bluetooth.requestDevice({
      filters: [
        {
          services: [LANDLINK_SERVICE_UUID],
          namePrefix: LANDLINK_DEVICE_NAME_PREFIX,
        },
        {
          services: [MESHTASTIC_SERVICE_UUID],
          namePrefix: MESHTASTIC_DEVICE_NAME_PREFIX,
        },
        // Meshtastic firmware lets users override the device name. Allow a
        // bare service-UUID filter as fallback so renamed devices still show.
        { services: [MESHTASTIC_SERVICE_UUID] },
      ],
      optionalServices: [LANDLINK_SERVICE_UUID, MESHTASTIC_SERVICE_UUID],
    });
    console.log("[ble] requestViaWeb: picked", {
      id: device.id,
      name: device.name,
    });
    webDeviceCache.set(device.id, device);
    return { id: device.id, name: device.name ?? "Unknown device" };
  } catch (err) {
    if (isWebUserCancellation(err)) throw new PairingCancelledError();
    console.warn("[ble] requestViaWeb failed", err);
    throw err;
  }
}

export function requestLandlinkDevice(): Promise<PairedDeviceInfo> {
  if (Capacitor.isNativePlatform()) return requestViaNative();
  return requestViaWeb();
}

// WHY: Web Bluetooth lets us re-acquire previously-permitted devices without a
// user gesture via getDevices(). Native is permission-by-id, so we just signal
// "the OS knows about this id" by returning empty (callers use stored ids).
export async function listPermittedDevices(): Promise<PairedDeviceInfo[]> {
  if (Capacitor.isNativePlatform()) return [];
  if (typeof navigator === "undefined" || !navigator.bluetooth) return [];
  const bt = navigator.bluetooth as Bluetooth & {
    getDevices?: () => Promise<BluetoothDevice[]>;
  };
  if (typeof bt.getDevices !== "function") return [];
  try {
    const devices = await bt.getDevices();
    const out: PairedDeviceInfo[] = [];
    for (const device of devices) {
      webDeviceCache.set(device.id, device);
      out.push({ id: device.id, name: device.name ?? "Unknown device" });
    }
    return out;
  } catch {
    return [];
  }
}

export async function reconnectLandlinkDevice(deviceId: string): Promise<void> {
  if (Capacitor.isNativePlatform()) {
    await connectLandlinkDevice(deviceId);
    return;
  }
  if (!webDeviceCache.has(deviceId)) {
    await listPermittedDevices();
  }
  if (!webDeviceCache.has(deviceId)) {
    throw new Error("Device permission lost. Pair again.");
  }
  await connectLandlinkDevice(deviceId);
}

export async function connectLandlinkDevice(deviceId: string): Promise<void> {
  console.log("[ble] connect start", { deviceId, native: Capacitor.isNativePlatform() });
  if (Capacitor.isNativePlatform()) {
    try {
      await BleClient.connect(deviceId, () => {
        console.warn("[ble] disconnect callback fired", { deviceId });
        fireDisconnect(deviceId);
      });
      console.log("[ble] native connect OK", { deviceId });
    } catch (err) {
      console.warn("[ble] native connect failed", { deviceId, err });
      if (isLikelyPairingError(err)) throw new PairingPinRequiredError();
      throw err;
    }
    // Meshtastic devices ship with BLE bonding enabled (fixed passkey 123456
    // by default, or a random passkey shown on the OLED). On Android we have
    // to explicitly request a bond to surface the system PIN dialog; iOS
    // auto-bonds when an encrypted characteristic is accessed. createBond
    // failure on iOS is expected ("not supported"); we report a clearer
    // error only if the failure looks like a PIN-related one on Android.
    try {
      const bonded = await BleClient.isBonded(deviceId);
      console.log("[ble] isBonded", { deviceId, bonded });
      if (!bonded) {
        console.log("[ble] createBond starting (waiting for OS PIN dialog)");
        await BleClient.createBond(deviceId, { timeout: 30000 });
        console.log("[ble] createBond OK");
      }
    } catch (err) {
      console.warn("[ble] bond flow failed", { deviceId, err });
      if (isLikelyPairingError(err)) {
        throw new PairingPinRequiredError();
      }
      // best-effort: continue and let characteristic access trigger pairing
      // implicitly if the OS supports it.
    }
    return;
  }
  const device = webDeviceCache.get(deviceId);
  if (!device?.gatt) {
    throw new Error("Device handle lost. Restart pairing.");
  }
  // Clear stale cache before reconnecting.
  clearWebCache(deviceId);
  let server: BluetoothRemoteGATTServer;
  try {
    console.log("[ble] web gatt.connect()");
    server = await device.gatt.connect();
    console.log("[ble] web gatt connected", {
      deviceId,
      connected: server.connected,
    });
  } catch (err) {
    console.warn("[ble] web gatt.connect failed", { deviceId, err });
    if (isLikelyPairingError(err)) throw new PairingPinRequiredError();
    throw err;
  }
  const handler = (): void => {
    console.warn("[ble] gattserverdisconnected", { deviceId });
    clearWebCache(deviceId);
    fireDisconnect(deviceId);
  };
  device.addEventListener("gattserverdisconnected", handler);
  webGattCache.set(deviceId, {
    server,
    services: new Map(),
    chars: new Map(),
    notifyHandlers: new Map(),
    disconnectHandler: handler,
  });
}

function clearWebCache(deviceId: string): void {
  const entry = webGattCache.get(deviceId);
  if (!entry) return;
  const device = webDeviceCache.get(deviceId);
  if (device && entry.disconnectHandler) {
    device.removeEventListener("gattserverdisconnected", entry.disconnectHandler);
  }
  webGattCache.delete(deviceId);
}

async function getWebChar(
  deviceId: string,
  serviceUuid: string,
  charUuid: string,
): Promise<BluetoothRemoteGATTCharacteristic> {
  const entry = webGattCache.get(deviceId);
  if (!entry) {
    throw new Error("Not connected. Call connectLandlinkDevice first.");
  }
  if (!entry.server.connected) {
    throw new Error("GATT server is disconnected.");
  }
  let service = entry.services.get(serviceUuid);
  if (!service) {
    service = await entry.server.getPrimaryService(serviceUuid);
    entry.services.set(serviceUuid, service);
  }
  const cached = entry.chars.get(charUuid);
  if (cached) return cached;
  const char = await service.getCharacteristic(charUuid);
  entry.chars.set(charUuid, char);
  return char;
}

// Probe which adapter to use post-connect. Returns the service UUID that the
// device exposes, or null if neither is found.
export async function detectDeviceProtocol(
  deviceId: string,
): Promise<string | null> {
  console.log("[ble] detectDeviceProtocol", { deviceId });
  if (Capacitor.isNativePlatform()) {
    let services: { uuid: string }[] = [];
    try {
      services = await BleClient.getServices(deviceId);
    } catch (err) {
      console.warn("[ble] getServices failed", err);
      return null;
    }
    console.log(
      "[ble] native services advertised",
      services.map((s) => s.uuid),
    );
    for (const svc of SUPPORTED_SERVICE_UUIDS) {
      if (services.some((s) => s.uuid.toLowerCase() === svc.toLowerCase())) {
        console.log("[ble] detected protocol service", svc);
        return svc;
      }
    }
    console.warn("[ble] no supported service on this device");
    return null;
  }
  const entry = webGattCache.get(deviceId);
  if (!entry?.server.connected) {
    console.warn("[ble] detectDeviceProtocol: no connected entry");
    return null;
  }
  for (const svc of SUPPORTED_SERVICE_UUIDS) {
    try {
      const handle = await entry.server.getPrimaryService(svc);
      entry.services.set(svc, handle);
      console.log("[ble] detected protocol service", svc);
      return svc;
    } catch (err) {
      console.log("[ble] service not present, trying next", { svc, err });
      // not this one; try next
    }
  }
  console.warn("[ble] no supported service on this device");
  return null;
}

export async function readCharacteristic(
  deviceId: string,
  serviceUuid: string,
  charUuid: string,
): Promise<Uint8Array> {
  try {
    if (Capacitor.isNativePlatform()) {
      const dv = await BleClient.read(deviceId, serviceUuid, charUuid);
      return dvToU8(dv);
    }
    const char = await getWebChar(deviceId, serviceUuid, charUuid);
    const dv = await char.readValue();
    return dvToU8(dv);
  } catch (err) {
    if (isLikelyPairingError(err)) throw new PairingPinRequiredError();
    throw err;
  }
}

export async function writeCharacteristic(
  deviceId: string,
  serviceUuid: string,
  charUuid: string,
  value: Uint8Array,
): Promise<void> {
  try {
    if (Capacitor.isNativePlatform()) {
      await BleClient.write(deviceId, serviceUuid, charUuid, u8ToDv(value));
      return;
    }
    const char = await getWebChar(deviceId, serviceUuid, charUuid);
    // WHY: with-response gives backpressure and surfaces firmware NACK; the
    // CMD characteristic is a request channel so reliability beats throughput.
    // WHY copy: BufferSource requires ArrayBuffer-backed views; Uint8Array
    // buffer is ArrayBufferLike which TS rejects when SharedArrayBuffer is in
    // lib.
    const buf = new ArrayBuffer(value.byteLength);
    new Uint8Array(buf).set(value);
    await char.writeValueWithResponse(buf);
  } catch (err) {
    if (isLikelyPairingError(err)) throw new PairingPinRequiredError();
    throw err;
  }
}

export async function startNotifications(
  deviceId: string,
  serviceUuid: string,
  charUuid: string,
  cb: (data: Uint8Array) => void,
): Promise<() => Promise<void>> {
  try {
    if (Capacitor.isNativePlatform()) {
      await BleClient.startNotifications(
        deviceId,
        serviceUuid,
        charUuid,
        (dv) => {
          cb(dvToU8(dv));
        },
      );
      return async () => {
        await BleClient.stopNotifications(deviceId, serviceUuid, charUuid);
      };
    }
    const char = await getWebChar(deviceId, serviceUuid, charUuid);
    const handler = (ev: Event): void => {
      const target = ev.target as BluetoothRemoteGATTCharacteristic | null;
      const dv = target?.value;
      if (!dv) return;
      cb(dvToU8(dv));
    };
    char.addEventListener("characteristicvaluechanged", handler);
    await char.startNotifications();
    const entry = webGattCache.get(deviceId);
    entry?.notifyHandlers.set(charUuid, handler);
    return async () => {
      char.removeEventListener("characteristicvaluechanged", handler);
      entry?.notifyHandlers.delete(charUuid);
      try {
        await char.stopNotifications();
      } catch {
        // device may have already disconnected
      }
    };
  } catch (err) {
    if (isLikelyPairingError(err)) throw new PairingPinRequiredError();
    throw err;
  }
}

export async function disconnect(deviceId: string): Promise<void> {
  if (Capacitor.isNativePlatform()) {
    try {
      await BleClient.disconnect(deviceId);
    } finally {
      fireDisconnect(deviceId);
    }
    return;
  }
  const entry = webGattCache.get(deviceId);
  if (!entry) return;
  try {
    if (entry.server.connected) entry.server.disconnect();
  } finally {
    clearWebCache(deviceId);
    fireDisconnect(deviceId);
  }
}

export function onDisconnect(deviceId: string, cb: () => void): () => void {
  let set = disconnectListeners.get(deviceId);
  if (!set) {
    set = new Set();
    disconnectListeners.set(deviceId, set);
  }
  set.add(cb);
  return () => {
    set.delete(cb);
    if (set.size === 0) disconnectListeners.delete(deviceId);
  };
}

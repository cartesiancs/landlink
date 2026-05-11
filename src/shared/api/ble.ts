import { Capacitor } from "@capacitor/core";
import { BleClient } from "@capacitor-community/bluetooth-le";

import {
  LANDLINK_DEVICE_NAME_PREFIX,
  LANDLINK_SERVICE_UUID,
} from "@/shared/protocol";

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
  service: BluetoothRemoteGATTService | null;
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
    const device = await BleClient.requestDevice({
      services: [LANDLINK_SERVICE_UUID],
      namePrefix: LANDLINK_DEVICE_NAME_PREFIX,
    });
    return { id: device.deviceId, name: device.name ?? "Unknown device" };
  } catch (err) {
    if (isNativeUserCancellation(err)) throw new PairingCancelledError();
    throw err;
  }
}

async function requestViaWeb(): Promise<PairedDeviceInfo> {
  if (typeof navigator === "undefined" || !navigator.bluetooth) {
    throw new Error("Web Bluetooth is not available in this browser.");
  }
  try {
    const device = await navigator.bluetooth.requestDevice({
      filters: [
        {
          services: [LANDLINK_SERVICE_UUID],
          namePrefix: LANDLINK_DEVICE_NAME_PREFIX,
        },
      ],
      optionalServices: [LANDLINK_SERVICE_UUID],
    });
    webDeviceCache.set(device.id, device);
    return { id: device.id, name: device.name ?? "Unknown device" };
  } catch (err) {
    if (isWebUserCancellation(err)) throw new PairingCancelledError();
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
  if (Capacitor.isNativePlatform()) {
    await BleClient.connect(deviceId, () => {
      fireDisconnect(deviceId);
    });
    return;
  }
  const device = webDeviceCache.get(deviceId);
  if (!device?.gatt) {
    throw new Error("Device handle lost. Restart pairing.");
  }
  // Clear stale cache before reconnecting.
  clearWebCache(deviceId);
  const server = await device.gatt.connect();
  const handler = (): void => {
    clearWebCache(deviceId);
    fireDisconnect(deviceId);
  };
  device.addEventListener("gattserverdisconnected", handler);
  webGattCache.set(deviceId, {
    server,
    service: null,
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
  charUuid: string,
): Promise<BluetoothRemoteGATTCharacteristic> {
  const entry = webGattCache.get(deviceId);
  if (!entry) {
    throw new Error("Not connected. Call connectLandlinkDevice first.");
  }
  if (!entry.server.connected) {
    throw new Error("GATT server is disconnected.");
  }
  entry.service ??= await entry.server.getPrimaryService(LANDLINK_SERVICE_UUID);
  const cached = entry.chars.get(charUuid);
  if (cached) return cached;
  const char = await entry.service.getCharacteristic(charUuid);
  entry.chars.set(charUuid, char);
  return char;
}

export async function readCharacteristic(
  deviceId: string,
  serviceUuid: string,
  charUuid: string,
): Promise<Uint8Array> {
  if (Capacitor.isNativePlatform()) {
    const dv = await BleClient.read(deviceId, serviceUuid, charUuid);
    return dvToU8(dv);
  }
  const char = await getWebChar(deviceId, charUuid);
  const dv = await char.readValue();
  return dvToU8(dv);
}

export async function writeCharacteristic(
  deviceId: string,
  serviceUuid: string,
  charUuid: string,
  value: Uint8Array,
): Promise<void> {
  if (Capacitor.isNativePlatform()) {
    await BleClient.write(deviceId, serviceUuid, charUuid, u8ToDv(value));
    return;
  }
  const char = await getWebChar(deviceId, charUuid);
  // WHY: with-response gives backpressure and surfaces firmware NACK; the CMD
  // characteristic is a request channel so reliability beats throughput here.
  // WHY copy: BufferSource requires ArrayBuffer-backed views; Uint8Array buffer
  // is ArrayBufferLike which TS rejects when SharedArrayBuffer is in lib.
  const buf = new ArrayBuffer(value.byteLength);
  new Uint8Array(buf).set(value);
  await char.writeValueWithResponse(buf);
}

export async function startNotifications(
  deviceId: string,
  serviceUuid: string,
  charUuid: string,
  cb: (data: Uint8Array) => void,
): Promise<() => Promise<void>> {
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
  const char = await getWebChar(deviceId, charUuid);
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

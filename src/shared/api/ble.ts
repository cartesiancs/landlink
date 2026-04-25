import { Capacitor } from "@capacitor/core";
import { BleClient } from "@capacitor-community/bluetooth-le";

import {
  LANDLINK_DEVICE_NAME_PREFIX,
  LANDLINK_SERVICE_UUID,
} from "@/shared/protocol/uuids";

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
      acceptAllDevices: true,
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

export async function connectLandlinkDevice(deviceId: string): Promise<void> {
  if (Capacitor.isNativePlatform()) {
    await BleClient.connect(deviceId);
    return;
  }
  const device = webDeviceCache.get(deviceId);
  if (!device) {
    throw new Error("Device handle lost — restart pairing.");
  }
  await device.gatt?.connect();
}

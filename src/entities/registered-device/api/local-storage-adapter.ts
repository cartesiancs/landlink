import type { RegisteredDevice } from "../model/types";

export const STORAGE_KEY = "vision.registered-devices.v1";
const SCHEMA_VERSION = 1;

type Envelope = {
  version: number;
  devices: RegisteredDevice[];
};

function isRegisteredDevice(value: unknown): value is RegisteredDevice {
  if (typeof value !== "object" || value === null) return false;
  const v = value as Record<string, unknown>;
  return (
    typeof v["id"] === "string" &&
    typeof v["name"] === "string" &&
    (v["source"] === "ble" || v["source"] === "mock") &&
    typeof v["enabled"] === "boolean" &&
    (v["status"] === "connected" || v["status"] === "disconnected") &&
    (v["pingMs"] === null || typeof v["pingMs"] === "number") &&
    (v["signalDbm"] === null || typeof v["signalDbm"] === "number") &&
    (v["lastConnectedAt"] === null ||
      typeof v["lastConnectedAt"] === "number") &&
    typeof v["registeredAt"] === "number"
  );
}

function getStorage(): Storage | null {
  try {
    return typeof window === "undefined" ? null : window.localStorage;
  } catch {
    return null;
  }
}

export function loadDevices(): RegisteredDevice[] {
  const storage = getStorage();
  if (!storage) return [];
  const raw = storage.getItem(STORAGE_KEY);
  if (raw === null) return [];
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw);
  } catch {
    console.warn("[registered-device] malformed JSON in storage; resetting");
    return [];
  }
  if (typeof parsed !== "object" || parsed === null) return [];
  const env = parsed as Partial<Envelope>;
  if (env.version !== SCHEMA_VERSION) return [];
  if (!Array.isArray(env.devices)) return [];
  return env.devices.filter(isRegisteredDevice);
}

export function saveDevices(devices: readonly RegisteredDevice[]): void {
  const storage = getStorage();
  if (!storage) return;
  const env: Envelope = { version: SCHEMA_VERSION, devices: [...devices] };
  try {
    storage.setItem(STORAGE_KEY, JSON.stringify(env));
  } catch (err) {
    console.warn("[registered-device] failed to persist", err);
  }
}

export function clearStoredDevices(): void {
  const storage = getStorage();
  if (!storage) return;
  storage.removeItem(STORAGE_KEY);
}

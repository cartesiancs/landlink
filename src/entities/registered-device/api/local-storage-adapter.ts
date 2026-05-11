import type { RegisteredDevice } from "../model/types";

export const STORAGE_KEY = "vision.registered-devices.v2";
const LEGACY_STORAGE_KEY_V1 = "vision.registered-devices.v1";
const SCHEMA_VERSION = 2;

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
    typeof v["registeredAt"] === "number" &&
    (v["nodeId"] === null || typeof v["nodeId"] === "string")
  );
}

function getStorage(): Storage | null {
  try {
    return typeof window === "undefined" ? null : window.localStorage;
  } catch {
    return null;
  }
}

function migrateLegacyV1(storage: Storage): RegisteredDevice[] {
  const raw = storage.getItem(LEGACY_STORAGE_KEY_V1);
  if (raw === null) return [];
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw);
  } catch {
    storage.removeItem(LEGACY_STORAGE_KEY_V1);
    return [];
  }
  if (typeof parsed !== "object" || parsed === null) {
    storage.removeItem(LEGACY_STORAGE_KEY_V1);
    return [];
  }
  const env = parsed as { version?: unknown; devices?: unknown };
  if (env.version !== 1 || !Array.isArray(env.devices)) {
    storage.removeItem(LEGACY_STORAGE_KEY_V1);
    return [];
  }
  const upgraded: RegisteredDevice[] = [];
  for (const entry of env.devices) {
    if (typeof entry !== "object" || entry === null) continue;
    const candidate = { ...(entry as Record<string, unknown>), nodeId: null };
    if (isRegisteredDevice(candidate)) {
      upgraded.push(candidate);
    }
  }
  const next: Envelope = { version: SCHEMA_VERSION, devices: upgraded };
  try {
    storage.setItem(STORAGE_KEY, JSON.stringify(next));
    storage.removeItem(LEGACY_STORAGE_KEY_V1);
  } catch (err) {
    console.warn("[registered-device] v1->v2 migration failed", err);
  }
  return upgraded;
}

export function loadDevices(): RegisteredDevice[] {
  const storage = getStorage();
  if (!storage) return [];
  const raw = storage.getItem(STORAGE_KEY);
  if (raw === null) {
    return migrateLegacyV1(storage);
  }
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
  storage.removeItem(LEGACY_STORAGE_KEY_V1);
}

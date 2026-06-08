import {
  hexToNodeNum,
  legacyLEHexToNodeNum,
  nodeNumToHex,
} from "@/shared/lib";

import type {
  RegisteredDevice,
  RegisteredDeviceProtocol,
} from "../model/types";

export const STORAGE_KEY = "vision.registered-devices.v3";
const LEGACY_STORAGE_KEY_V2 = "vision.registered-devices.v2";
const LEGACY_STORAGE_KEY_V1 = "vision.registered-devices.v1";
const SCHEMA_VERSION = 3;

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
    (v["nodeNum"] === null || typeof v["nodeNum"] === "number") &&
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

// Decide how to interpret a legacy v2 nodeId hex string. Meshtastic-sourced
// entries stored BE canonical hex already; Landlink-sourced ones stored
// LE-byte-order hex from the pre-normalisation parsers.
function v2NodeNumOf(
  protocol: RegisteredDeviceProtocol | undefined,
  nodeId: string,
): number | null {
  if (protocol === "meshtastic") return hexToNodeNum(nodeId);
  return legacyLEHexToNodeNum(nodeId);
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
    const candidate = {
      ...(entry as Record<string, unknown>),
      nodeNum: null,
      nodeId: null,
    };
    if (isRegisteredDevice(candidate)) {
      upgraded.push(candidate);
    }
  }
  const next: Envelope = { version: SCHEMA_VERSION, devices: upgraded };
  try {
    storage.setItem(STORAGE_KEY, JSON.stringify(next));
    storage.removeItem(LEGACY_STORAGE_KEY_V1);
  } catch (err) {
    console.warn("[registered-device] v1->v3 migration failed", err);
  }
  return upgraded;
}

function migrateLegacyV2(storage: Storage): RegisteredDevice[] {
  const raw = storage.getItem(LEGACY_STORAGE_KEY_V2);
  if (raw === null) return [];
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw);
  } catch {
    storage.removeItem(LEGACY_STORAGE_KEY_V2);
    return [];
  }
  if (typeof parsed !== "object" || parsed === null) {
    storage.removeItem(LEGACY_STORAGE_KEY_V2);
    return [];
  }
  const env = parsed as { version?: unknown; devices?: unknown };
  if (env.version !== 2 || !Array.isArray(env.devices)) {
    storage.removeItem(LEGACY_STORAGE_KEY_V2);
    return [];
  }
  const upgraded: RegisteredDevice[] = [];
  for (const entry of env.devices) {
    if (typeof entry !== "object" || entry === null) continue;
    const src = entry as Record<string, unknown>;
    const proto: RegisteredDeviceProtocol | undefined =
      src["protocol"] === "meshtastic"
        ? "meshtastic"
        : src["protocol"] === "landlink"
        ? "landlink"
        : undefined;
    let nodeNum: number | null = null;
    let nodeId: string | null = null;
    if (typeof src["nodeId"] === "string" && src["nodeId"].length === 8) {
      const n = v2NodeNumOf(proto, src["nodeId"]);
      if (n !== null) {
        nodeNum = n;
        nodeId = nodeNumToHex(n);
      }
    }
    const candidate = {
      ...src,
      nodeNum,
      nodeId,
    };
    if (isRegisteredDevice(candidate)) {
      upgraded.push(candidate);
    }
  }
  const next: Envelope = { version: SCHEMA_VERSION, devices: upgraded };
  try {
    storage.setItem(STORAGE_KEY, JSON.stringify(next));
    storage.removeItem(LEGACY_STORAGE_KEY_V2);
  } catch (err) {
    console.warn("[registered-device] v2->v3 migration failed", err);
  }
  return upgraded;
}

export function loadDevices(): RegisteredDevice[] {
  const storage = getStorage();
  if (!storage) return [];
  const raw = storage.getItem(STORAGE_KEY);
  if (raw === null) {
    const v2 = migrateLegacyV2(storage);
    if (v2.length > 0) return v2;
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
  storage.removeItem(LEGACY_STORAGE_KEY_V2);
  storage.removeItem(LEGACY_STORAGE_KEY_V1);
}

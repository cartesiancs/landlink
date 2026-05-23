import { FIRMWARE_MANIFEST_URL } from "@/shared/config";

import type {
  FirmwareAsset,
  FirmwareAssetRole,
  FirmwareRelease,
  FirmwareReleaseAssets,
} from "../model/types";

function asString(value: unknown): string | null {
  return typeof value === "string" ? value : null;
}

function asNumber(value: unknown): number | null {
  return typeof value === "number" && Number.isFinite(value) ? value : null;
}

function asRecord(value: unknown): Record<string, unknown> | null {
  return typeof value === "object" && value !== null && !Array.isArray(value)
    ? (value as Record<string, unknown>)
    : null;
}

function parseAsset(role: FirmwareAssetRole, raw: unknown): FirmwareAsset | null {
  const obj = asRecord(raw);
  if (!obj) return null;
  const name = asString(obj["name"]);
  const size = asNumber(obj["size"]);
  const downloadUrl = asString(obj["downloadUrl"]);
  if (!name || size === null || !downloadUrl) return null;
  return { role, name, size, downloadUrl };
}

function parseAssets(raw: unknown): FirmwareReleaseAssets | null {
  const obj = asRecord(raw);
  if (!obj) return null;
  const firmware = parseAsset("firmware", obj["firmware"]);
  const bootloader = parseAsset("bootloader", obj["bootloader"]);
  const partitions = parseAsset("partitions", obj["partitions"]);
  if (!firmware || !bootloader || !partitions) return null;
  return { firmware, bootloader, partitions };
}

function parseRelease(raw: unknown): FirmwareRelease | null {
  const obj = asRecord(raw);
  if (!obj) return null;
  const version = asString(obj["version"]);
  const tag = asString(obj["tag"]);
  const channelRaw = asString(obj["channel"]);
  const releasedAt = asString(obj["releasedAt"]);
  const notes = asString(obj["notes"]) ?? "";
  const assets = parseAssets(obj["assets"]);
  if (!version || !tag || !releasedAt || !assets) return null;
  const channel = channelRaw === "beta" ? "beta" : "stable";
  return { version, tag, channel, releasedAt, notes, assets };
}

export async function fetchFirmwareReleases(
  signal?: AbortSignal,
): Promise<FirmwareRelease[]> {
  const init: RequestInit = { headers: { Accept: "application/json" } };
  if (signal) init.signal = signal;

  const res = await fetch(FIRMWARE_MANIFEST_URL, init);
  if (!res.ok) {
    throw new Error(`Manifest request failed: ${String(res.status)}`);
  }

  const json: unknown = await res.json();
  if (!Array.isArray(json)) return [];

  const out: FirmwareRelease[] = [];
  for (const item of json) {
    const release = parseRelease(item);
    if (release) out.push(release);
  }
  return out;
}

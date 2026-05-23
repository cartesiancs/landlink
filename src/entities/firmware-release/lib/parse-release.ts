import { FIRMWARE_TAG_PREFIX } from "@/shared/config";

import type {
  FirmwareAsset,
  FirmwareAssetRole,
  FirmwareRelease,
} from "../model/types";

type GitHubAsset = {
  name: string;
  size: number;
  browser_download_url: string;
};

type GitHubRelease = {
  tag_name: string;
  name?: string | null;
  body?: string | null;
  draft: boolean;
  prerelease: boolean;
  published_at?: string | null;
  created_at: string;
  assets: GitHubAsset[];
};

function classify(name: string): FirmwareAssetRole | null {
  if (name.startsWith("landlink-module1-") && name.endsWith(".bin")) {
    return "firmware";
  }
  if (name.startsWith("bootloader-") && name.endsWith(".bin")) {
    return "bootloader";
  }
  if (name.startsWith("partitions-") && name.endsWith(".bin")) {
    return "partitions";
  }
  return null;
}

function toAsset(role: FirmwareAssetRole, gh: GitHubAsset): FirmwareAsset {
  return {
    role,
    name: gh.name,
    size: gh.size,
    downloadUrl: gh.browser_download_url,
  };
}

export function parseRelease(raw: unknown): FirmwareRelease | null {
  if (typeof raw !== "object" || raw === null) return null;
  const gh = raw as GitHubRelease;

  if (gh.draft) return null;
  if (typeof gh.tag_name !== "string") return null;
  if (!gh.tag_name.startsWith(FIRMWARE_TAG_PREFIX)) return null;

  let firmware: FirmwareAsset | null = null;
  let bootloader: FirmwareAsset | null = null;
  let partitions: FirmwareAsset | null = null;

  for (const asset of gh.assets) {
    const role = classify(asset.name);
    if (role === "firmware") firmware = toAsset(role, asset);
    else if (role === "bootloader") bootloader = toAsset(role, asset);
    else if (role === "partitions") partitions = toAsset(role, asset);
  }

  if (!firmware || !bootloader || !partitions) return null;

  const version = gh.tag_name.slice(FIRMWARE_TAG_PREFIX.length);
  const releasedAt = gh.published_at ?? gh.created_at;

  return {
    version,
    tag: gh.tag_name,
    channel: gh.prerelease ? "beta" : "stable",
    releasedAt,
    notes: gh.body ?? "",
    assets: { firmware, bootloader, partitions },
  };
}

export function parseReleases(raw: unknown): FirmwareRelease[] {
  if (!Array.isArray(raw)) return [];
  const out: FirmwareRelease[] = [];
  for (const item of raw) {
    const release = parseRelease(item);
    if (release) out.push(release);
  }
  return out;
}

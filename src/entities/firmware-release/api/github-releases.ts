import { FIRMWARE_RELEASES_URL } from "@/shared/config";

import { parseReleases } from "../lib/parse-release";
import type { FirmwareRelease } from "../model/types";

export async function fetchFirmwareReleases(
  signal?: AbortSignal,
): Promise<FirmwareRelease[]> {
  const init: RequestInit = {
    headers: {
      Accept: "application/vnd.github+json",
      "X-GitHub-Api-Version": "2022-11-28",
    },
  };
  if (signal) init.signal = signal;

  const res = await fetch(FIRMWARE_RELEASES_URL, init);

  if (!res.ok) {
    throw new Error(`GitHub releases request failed: ${String(res.status)}`);
  }

  const json: unknown = await res.json();
  return parseReleases(json);
}

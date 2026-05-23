import { useCallback, useEffect, useState } from "react";

import { fetchFirmwareReleases } from "../api/github-releases";
import type { FirmwareRelease } from "./types";

export type FirmwareReleasesStatus = "idle" | "loading" | "ok" | "error";

export type FirmwareReleasesState = {
  status: FirmwareReleasesStatus;
  releases: FirmwareRelease[];
  error: string | null;
  reload: () => void;
};

let cache: FirmwareRelease[] | null = null;

function describe(err: unknown): string {
  if (err instanceof Error && err.message) return err.message;
  return "Couldn't load firmware releases.";
}

export function useFirmwareReleases(): FirmwareReleasesState {
  const [status, setStatus] = useState<FirmwareReleasesStatus>(() =>
    cache ? "ok" : "loading",
  );
  const [releases, setReleases] = useState<FirmwareRelease[]>(() => cache ?? []);
  const [error, setError] = useState<string | null>(null);
  const [reloadKey, setReloadKey] = useState(0);

  useEffect(() => {
    if (cache && reloadKey === 0) {
      return;
    }

    const controller = new AbortController();
    let cancelled = false;

    fetchFirmwareReleases(controller.signal)
      .then((result) => {
        if (cancelled) return;
        cache = result;
        setReleases(result);
        setStatus("ok");
      })
      .catch((err: unknown) => {
        if (cancelled) return;
        if (err instanceof DOMException && err.name === "AbortError") return;
        setError(describe(err));
        setStatus("error");
      });

    return () => {
      cancelled = true;
      controller.abort();
    };
  }, [reloadKey]);

  const reload = useCallback(() => {
    cache = null;
    setStatus("loading");
    setError(null);
    setReleases([]);
    setReloadKey((k) => k + 1);
  }, []);

  return { status, releases, error, reload };
}

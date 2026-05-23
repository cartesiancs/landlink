import { useCallback, useEffect, useRef, useState } from "react";

import type { FirmwareRelease } from "@/entities/firmware-release";

import {
  closeFlasher,
  flashRelease,
  openFlasher,
  type FlasherHandle,
} from "../api/esptool-client";
import { isWebSerialSupported } from "../lib/web-serial-supported";
import { FlashCancelledError, type FlashStatus } from "./types";

export type UseFirmwareFlash = {
  status: FlashStatus;
  progress: number | null;
  chip: string | null;
  error: string | null;
  isSupported: boolean;
  connect: () => Promise<void>;
  disconnect: () => Promise<void>;
  flash: (release: FirmwareRelease) => Promise<void>;
};

function describe(err: unknown): string {
  if (err instanceof Error && err.message) return err.message;
  return "Something went wrong while flashing.";
}

export function useFirmwareFlash(): UseFirmwareFlash {
  const isSupported = isWebSerialSupported();
  const [status, setStatus] = useState<FlashStatus>(
    isSupported ? "idle" : "unsupported",
  );
  const [progress, setProgress] = useState<number | null>(null);
  const [chip, setChip] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  const handleRef = useRef<FlasherHandle | null>(null);
  const mountedRef = useRef(true);

  useEffect(() => {
    return () => {
      mountedRef.current = false;
      const handle = handleRef.current;
      handleRef.current = null;
      if (handle) {
        void closeFlasher(handle);
      }
    };
  }, []);

  const connect = useCallback(async () => {
    if (!isSupported) return;
    if (handleRef.current) return;

    setStatus("connecting");
    setError(null);
    setProgress(null);

    try {
      const handle = await openFlasher();
      if (!mountedRef.current) {
        await closeFlasher(handle);
        return;
      }
      handleRef.current = handle;
      setChip(handle.chip);
      setStatus("connected");
    } catch (err) {
      if (err instanceof FlashCancelledError) {
        setStatus("idle");
        return;
      }
      setError(describe(err));
      setStatus("error");
    }
  }, [isSupported]);

  const disconnect = useCallback(async () => {
    const handle = handleRef.current;
    handleRef.current = null;
    if (handle) await closeFlasher(handle);
    if (!mountedRef.current) return;
    setChip(null);
    setProgress(null);
    setStatus("idle");
    setError(null);
  }, []);

  const flash = useCallback(async (release: FirmwareRelease) => {
    const handle = handleRef.current;
    if (!handle) return;

    setStatus("flashing");
    setProgress(0);
    setError(null);

    try {
      await flashRelease(handle, release, (p) => {
        if (mountedRef.current) setProgress(p.percent);
      });
      if (!mountedRef.current) return;
      setProgress(100);
      setStatus("done");
    } catch (err) {
      if (!mountedRef.current) return;
      setError(describe(err));
      setStatus("error");
    } finally {
      const current = handleRef.current;
      handleRef.current = null;
      if (current) {
        void closeFlasher(current);
      }
    }
  }, []);

  return {
    status,
    progress,
    chip,
    error,
    isSupported,
    connect,
    disconnect,
    flash,
  };
}

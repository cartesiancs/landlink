import { useCallback, useState } from "react";

import {
  landlinkChannelSet,
  useLandlinkDevice,
} from "@/entities/landlink-device";
import {
  generatePsk,
  nextFreeIndex,
} from "@/entities/meshtastic-channel";

// Meshtastic Channel.Settings.name caps at 12 bytes; we enforce char count
// against UTF-8 byte length to avoid silent truncation on multi-byte names.
const MAX_NAME_BYTES = 12;

export type CreateChannelStatus = "idle" | "creating" | "success" | "error";

export function useCreateChannel() {
  const [status, setStatus] = useState<CreateChannelStatus>("idle");
  const [error, setError] = useState<string | null>(null);
  const device = useLandlinkDevice();

  const create = useCallback(async (name: string): Promise<boolean> => {
    const trimmed = name.trim();
    setError(null);

    if (!device) {
      setStatus("error");
      setError("No device connected");
      return false;
    }
    if (trimmed.length === 0) {
      setStatus("error");
      setError("Name is required");
      return false;
    }
    const byteLength = new TextEncoder().encode(trimmed).byteLength;
    if (byteLength > MAX_NAME_BYTES) {
      setStatus("error");
      setError(`Name exceeds ${MAX_NAME_BYTES.toString()} bytes`);
      return false;
    }

    const index = nextFreeIndex(device.deviceId);
    if (index === null) {
      setStatus("error");
      setError("All 7 channel slots are in use");
      return false;
    }

    setStatus("creating");
    try {
      // Write to the device's channel registry. The sync feature listens for
      // the CHANNEL_RESULT EVT and updates the local store, so we don't
      // mutate cache state here — that keeps the device authoritative and
      // prevents the UI from drifting if the firmware rejected the slot.
      await landlinkChannelSet(index, trimmed, generatePsk(), "secondary");
      setStatus("success");
      return true;
    } catch (err) {
      setStatus("error");
      setError(err instanceof Error ? err.message : "Create failed");
      return false;
    }
  }, [device]);

  const reset = useCallback(() => {
    setStatus("idle");
    setError(null);
  }, []);

  return {
    status,
    error,
    create,
    reset,
    maxNameBytes: MAX_NAME_BYTES,
    canCreate: device !== null,
  };
}

import { useCallback, useState } from "react";

import {
  landlinkChannelSet,
  useLandlinkDevice,
} from "@/entities/landlink-device";
import {
  nextFreeIndex,
  pskFromString,
} from "@/entities/meshtastic-channel";

const MAX_NAME_BYTES = 12;
// Meshtastic ChannelSettings.psk accepts 0, 1, 16, or 32 bytes (empty/short =
// "use default key"; 16 = AES-128; 32 = AES-256). We reject anything else so
// the firmware doesn't silently fall back to a default key.
const ALLOWED_PSK_LENGTHS = new Set([0, 1, 16, 32]);

export type ImportChannelStatus = "idle" | "importing" | "success" | "error";

export function useImportChannel() {
  const [status, setStatus] = useState<ImportChannelStatus>("idle");
  const [error, setError] = useState<string | null>(null);
  const device = useLandlinkDevice();

  const importChannel = useCallback(
    async (name: string, channelKey: string): Promise<boolean> => {
      const trimmedName = name.trim();
      setError(null);

      if (!device) {
        setStatus("error");
        setError("No device connected");
        return false;
      }
      if (trimmedName.length === 0) {
        setStatus("error");
        setError("Name is required");
        return false;
      }
      const byteLength = new TextEncoder().encode(trimmedName).byteLength;
      if (byteLength > MAX_NAME_BYTES) {
        setStatus("error");
        setError(`Name exceeds ${MAX_NAME_BYTES.toString()} bytes`);
        return false;
      }

      let psk: Uint8Array;
      try {
        psk = pskFromString(channelKey);
      } catch (err) {
        setStatus("error");
        setError(err instanceof Error ? err.message : "Invalid channel key");
        return false;
      }
      if (!ALLOWED_PSK_LENGTHS.has(psk.byteLength)) {
        setStatus("error");
        setError(
          `Channel key must decode to 16 or 32 bytes (got ${psk.byteLength.toString()})`,
        );
        return false;
      }

      const index = nextFreeIndex(device.deviceId);
      if (index === null) {
        setStatus("error");
        setError("All 7 channel slots are in use");
        return false;
      }

      setStatus("importing");
      try {
        // Mirror create-channel: write to the device registry and let the sync
        // feature reflect the EVT back into the local store.
        await landlinkChannelSet(index, trimmedName, psk, "secondary");
        setStatus("success");
        return true;
      } catch (err) {
        setStatus("error");
        setError(err instanceof Error ? err.message : "Import failed");
        return false;
      }
    },
    [device],
  );

  const reset = useCallback(() => {
    setStatus("idle");
    setError(null);
  }, []);

  return {
    status,
    error,
    importChannel,
    reset,
    maxNameBytes: MAX_NAME_BYTES,
    canImport: device !== null,
  };
}

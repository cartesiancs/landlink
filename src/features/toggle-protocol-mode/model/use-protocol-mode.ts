import { useCallback, useState } from "react";

import {
  setLandlinkProtocolMode,
  useLandlinkDevice,
  type ProtocolMode,
} from "@/entities/landlink-device";

export type UseProtocolMode = {
  mode: ProtocolMode | null;
  isMeshtastic: boolean;
  isConnected: boolean;
  isPending: boolean;
  toggle: () => Promise<void>;
};

export function useProtocolMode(): UseProtocolMode {
  const device = useLandlinkDevice();
  const [pending, setPending] = useState(false);

  const mode: ProtocolMode | null = device?.protocol ?? null;
  const isConnected = device?.status === "connected";

  const toggle = useCallback(async () => {
    if (!isConnected) return;
    if (pending) return;
    const next: ProtocolMode = mode === 1 ? 0 : 1;
    setPending(true);
    try {
      await setLandlinkProtocolMode(next);
    } finally {
      setPending(false);
    }
  }, [isConnected, mode, pending]);

  return {
    mode,
    isMeshtastic: mode === 1,
    isConnected,
    isPending: pending,
    toggle,
  };
}

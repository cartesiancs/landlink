import { useCallback, useState } from "react";

import {
  setLandlinkRegion,
  useLandlinkDevice,
} from "@/entities/landlink-device";
import { isRegionValue } from "@/shared/config";
import { type RegionValue } from "@/shared/protocol";

export type UseRadioRegion = {
  region: RegionValue | null;
  isConnected: boolean;
  isPending: boolean;
  select: (next: RegionValue) => Promise<void>;
};

export function useRadioRegion(): UseRadioRegion {
  const device = useLandlinkDevice();
  const [pending, setPending] = useState(false);

  const raw = device?.info?.region ?? null;
  const region: RegionValue | null =
    raw !== null && isRegionValue(raw) ? raw : null;
  const isConnected = device?.status === "connected";

  const select = useCallback(
    async (next: RegionValue) => {
      if (!isConnected) return;
      if (pending) return;
      if (next === region) return;
      setPending(true);
      try {
        await setLandlinkRegion(next);
      } finally {
        setPending(false);
      }
    },
    [isConnected, pending, region],
  );

  return {
    region,
    isConnected,
    isPending: pending,
    select,
  };
}

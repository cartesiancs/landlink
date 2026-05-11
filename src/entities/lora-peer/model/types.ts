import type { ChargeState, GpsFix } from "@/entities/landlink-device";

export type LoraPeer = {
  nodeId: string;
  batteryPct: number | null;
  batteryMv: number | null;
  chargeState: ChargeState | null;
  rssiDbm: number | null;
  gps: GpsFix | null;
  lastSeenAt: number;
};

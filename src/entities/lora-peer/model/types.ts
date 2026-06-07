import type { ChargeState, GpsFix } from "@/entities/landlink-device";

// "beacon" — heard via LORA_PEER_FOUND, has fresh telemetry, expires on TTL.
// "chat"   — observed as the sender of an incoming mesh message; persists.
// "history" — reconstructed from persisted message history on startup.
export type LoraPeerSource = "beacon" | "chat" | "history";

export type LoraPeer = {
  nodeId: string;
  batteryPct: number | null;
  batteryMv: number | null;
  chargeState: ChargeState | null;
  rssiDbm: number | null;
  gps: GpsFix | null;
  lastSeenAt: number;
  source: LoraPeerSource;
};

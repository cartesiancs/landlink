import type { ChargeState, GpsFix } from "@/entities/landlink-device";

// "beacon" — heard via LORA_PEER_FOUND, has fresh telemetry, expires on TTL.
// "chat"   — observed as the sender of an incoming mesh message; persists.
// "history" — reconstructed from persisted message history on startup.
export type LoraPeerSource = "beacon" | "chat" | "history";

export type LoraPeer = {
  // Canonical numeric identity. Used as the store key and in every
  // comparator. Hex strings are display-only and derived from this.
  nodeNum: number;
  // BE canonical 8-char hex of `nodeNum`, kept on the peer so render sites
  // do not have to import the helper. Always equals `nodeNumToHex(nodeNum)`.
  nodeId: string;
  batteryPct: number | null;
  batteryMv: number | null;
  chargeState: ChargeState | null;
  rssiDbm: number | null;
  gps: GpsFix | null;
  lastSeenAt: number;
  source: LoraPeerSource;
};

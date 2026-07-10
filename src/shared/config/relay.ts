// Anonymous relay configuration. The relay is optional: when no URL is
// configured the app is BLE-only and every remote-connectivity code path
// no-ops. The URL points at the relay's base (WSS for the socket, HTTPS for
// the enroll endpoints — we normalize per use).

const RAW = import.meta.env.VITE_LANDLINK_RELAY_URL?.trim() ?? "";

export const RELAY_BASE_URL: string | null = RAW.length > 0 ? RAW : null;

export function isRelayConfigured(): boolean {
  return RELAY_BASE_URL !== null;
}

// WebSocket endpoint for the bidirectional opaque-frame relay.
export function relayWsUrl(): string | null {
  if (!RELAY_BASE_URL) return null;
  const base = RELAY_BASE_URL.replace(/\/+$/, "");
  const ws = base.replace(/^http/, "ws");
  return `${ws}/v1/relay`;
}

// HTTPS base for REST endpoints (challenge, device enrollment).
export function relayHttpBase(): string | null {
  if (!RELAY_BASE_URL) return null;
  const base = RELAY_BASE_URL.replace(/\/+$/, "");
  return base.replace(/^ws/, "http");
}

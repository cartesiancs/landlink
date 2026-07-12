// Anonymous relay configuration, stored at RUNTIME (localStorage) so the user can
// point the app at a relay and opt into remote connectivity. Remote relay is
// OPT-IN: it defaults OFF, and every remote code path no-ops until the user
// enables it with a valid URL. The URL is the relay's base (WSS for the socket,
// HTTPS for the enroll endpoints — normalized per use).

export type RelayConfig = {
  relayEnabled: boolean;
  relayUrl: string;
  // TCP port devices dial (host derived from relayUrl). Constrained devices use
  // a plain-TCP link (no TLS), separate from the account wss URL.
  relayDevicePort: number;
};

const STORAGE_KEY = "vision.relay-config.v1";
const DEFAULT_DEVICE_PORT = 9000;

// The build-time env value is only a DEFAULT for the URL field; it never enables
// the relay on its own.
const ENV_DEFAULT_URL = import.meta.env.VITE_LANDLINK_RELAY_URL?.trim() ?? "";

const DEFAULT_CONFIG: RelayConfig = {
  relayEnabled: false,
  relayUrl: ENV_DEFAULT_URL,
  relayDevicePort: DEFAULT_DEVICE_PORT,
};

let snapshot: RelayConfig | null = null;
const listeners = new Set<() => void>();

function getStorage(): Storage | null {
  try {
    return typeof window === "undefined" ? null : window.localStorage;
  } catch {
    return null;
  }
}

function load(): RelayConfig {
  const storage = getStorage();
  const raw = storage?.getItem(STORAGE_KEY);
  if (!raw) return { ...DEFAULT_CONFIG };
  try {
    const parsed: unknown = JSON.parse(raw);
    if (parsed && typeof parsed === "object") {
      const p = parsed as Partial<RelayConfig>;
      const port = p.relayDevicePort;
      return {
        relayEnabled: typeof p.relayEnabled === "boolean" ? p.relayEnabled : false,
        relayUrl: typeof p.relayUrl === "string" ? p.relayUrl : ENV_DEFAULT_URL,
        relayDevicePort:
          typeof port === "number" && port > 0 && port < 65536
            ? port
            : DEFAULT_DEVICE_PORT,
      };
    }
  } catch {
    // fall through to default
  }
  return { ...DEFAULT_CONFIG };
}

function persist(cfg: RelayConfig): void {
  try {
    getStorage()?.setItem(STORAGE_KEY, JSON.stringify(cfg));
  } catch {
    // ignore quota / disabled storage
  }
}

function ensureHydrated(): RelayConfig {
  snapshot ??= load();
  return snapshot;
}

export function getRelayConfig(): RelayConfig {
  return ensureHydrated();
}

export function subscribeRelayConfig(l: () => void): () => void {
  ensureHydrated();
  listeners.add(l);
  return () => {
    listeners.delete(l);
  };
}

export function setRelayConfig(patch: Partial<RelayConfig>): void {
  const current = ensureHydrated();
  const next: RelayConfig = {
    relayEnabled: patch.relayEnabled ?? current.relayEnabled,
    relayUrl: patch.relayUrl ?? current.relayUrl,
    relayDevicePort: patch.relayDevicePort ?? current.relayDevicePort,
  };
  if (
    next.relayEnabled === current.relayEnabled &&
    next.relayUrl === current.relayUrl &&
    next.relayDevicePort === current.relayDevicePort
  ) {
    return; // no-op keeps the snapshot identity stable
  }
  snapshot = next;
  persist(next);
  for (const l of listeners) l();
}

export function _resetRelayConfigStore(): void {
  snapshot = null;
  listeners.clear();
  try {
    getStorage()?.removeItem(STORAGE_KEY);
  } catch {
    // ignore
  }
}

// --- validation + derived endpoints ---------------------------------------

// A usable relay URL is an absolute ws/wss/http/https URL.
export function isValidRelayUrl(url: string): boolean {
  const trimmed = url.trim();
  if (trimmed.length === 0) return false;
  try {
    const u = new URL(trimmed);
    return (
      u.protocol === "ws:" ||
      u.protocol === "wss:" ||
      u.protocol === "http:" ||
      u.protocol === "https:"
    );
  } catch {
    return false;
  }
}

// Remote relay is usable only when the user enabled it AND set a valid URL.
export function isRelayConfigured(): boolean {
  const cfg = ensureHydrated();
  return cfg.relayEnabled && isValidRelayUrl(cfg.relayUrl);
}

// The configured relay base URL (trailing slashes stripped), or null when relay
// is off / not valid.
export function relayBaseUrl(): string | null {
  if (!isRelayConfigured()) return null;
  return ensureHydrated().relayUrl.trim().replace(/\/+$/, "");
}

// WebSocket endpoint for the bidirectional opaque-frame relay.
export function relayWsUrl(): string | null {
  const base = relayBaseUrl();
  if (!base) return null;
  const ws = base.replace(/^http/, "ws");
  return `${ws}/v1/relay`;
}

// HTTPS base for REST endpoints (challenge, device enrollment).
export function relayHttpBase(): string | null {
  const base = relayBaseUrl();
  if (!base) return null;
  return base.replace(/^ws/, "http");
}

// The device's plain-TCP endpoint `host:port` (host from relayUrl, port from
// relayDevicePort), pushed to the device at enroll. Null when relay is off.
export function relayDeviceEndpoint(): string | null {
  if (!isRelayConfigured()) return null;
  const cfg = ensureHydrated();
  try {
    const host = new URL(cfg.relayUrl).hostname;
    if (!host) return null;
    return `${host}:${cfg.relayDevicePort.toString()}`;
  } catch {
    return null;
  }
}

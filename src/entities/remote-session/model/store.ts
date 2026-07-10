// Relay connection status. Hand-rolled external store consistent with the rest
// of the app; consumed via useRelayStatus for the "Connected remotely" badge
// and settings surface.

export type RelayStatus = "offline" | "connecting" | "online" | "error";

export type RelayState = {
  status: RelayStatus;
  error: string | null;
};

let state: RelayState = { status: "offline", error: null };
const listeners = new Set<() => void>();

function emit(): void {
  for (const l of listeners) {
    try {
      l();
    } catch {
      // listeners must not break each other
    }
  }
}

export function getRelayState(): RelayState {
  return state;
}

export function subscribeRelayState(cb: () => void): () => void {
  listeners.add(cb);
  return () => {
    listeners.delete(cb);
  };
}

export function setRelayStatus(status: RelayStatus, error: string | null = null): void {
  if (state.status === status && state.error === error) return;
  state = { status, error };
  emit();
}

export function _resetRelayStore(): void {
  state = { status: "offline", error: null };
  listeners.clear();
}

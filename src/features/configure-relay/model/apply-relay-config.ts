import { closeRelaySession } from "@/entities/remote-session";
import { getRelayConfig, setRelayConfig, type RelayConfig } from "@/shared/config";

// Apply a relay-config change and run its side effects. When the relay is
// disabled, or the URL changes, close any live relay session so a stale socket
// (still pointing at the old URL) is dropped. The remote transport's onClose then
// detaches the device and auto-reconnect re-evaluates: BLE-only when disabled, or
// a fresh relay connect once the device is re-enrolled against the new URL.
//
// WHY not touch remoteEnrolled on a URL change: the device was provisioned with
// its relay URL over BLE, so a URL change means it dials the OLD relay until it is
// re-paired. We keep the enrollment (remote degrades to Bluetooth) and warn the
// user in the UI rather than silently invalidating every device.
export function applyRelayConfig(patch: Partial<RelayConfig>): void {
  const before = getRelayConfig();
  setRelayConfig(patch);
  const after = getRelayConfig();

  const urlChanged = before.relayUrl !== after.relayUrl;
  const disabled = before.relayEnabled && !after.relayEnabled;
  if (urlChanged || disabled) {
    closeRelaySession();
  }
}

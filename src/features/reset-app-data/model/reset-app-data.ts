import { resetAnonIdentity } from "@/entities/anon-identity";
import { _resetDebugModeStore, setDebugMode } from "@/entities/debug-mode";
import { clearAllMessages } from "@/entities/landlink-device";
import {
  _resetRegisteredDevicesStore,
  clearRegisteredDevices,
} from "@/entities/registered-device";
import { closeRelaySession } from "@/entities/remote-session";
import { _resetWifiStatusStore } from "@/entities/wifi-status";
import { _resetRelayConfigStore } from "@/shared/config";

const VISION_KEY_PREFIX = "vision.";

function getStorage(): Storage | null {
  try {
    return typeof window === "undefined" ? null : window.localStorage;
  } catch {
    return null;
  }
}

export function resetAppData(): { keysRemoved: number } {
  const storage = getStorage();
  let keysRemoved = 0;

  if (storage) {
    const toRemove: string[] = [];
    for (let i = 0; i < storage.length; i++) {
      const key = storage.key(i);
      if (key?.startsWith(VISION_KEY_PREFIX)) {
        toRemove.push(key);
      }
    }
    for (const key of toRemove) {
      storage.removeItem(key);
    }
    keysRemoved = toRemove.length;
  }

  clearRegisteredDevices();
  setDebugMode(false);
  _resetRegisteredDevicesStore();
  _resetDebugModeStore();
  _resetWifiStatusStore();
  _resetRelayConfigStore();
  // Persisted channel-chat history lives in IndexedDB, not localStorage,
  // so the prefix sweep above doesn't reach it. Fire-and-forget: any IDB
  // error is warn-logged inside clearAllMessages and must not block the
  // synchronous reset (the button caller already toasts success).
  void clearAllMessages();

  // The anonymous identity keypair lives in its own IndexedDB database, and the
  // relay socket must drop when its account key is being wiped. Both are
  // fire-and-forget for the same reason as clearAllMessages above.
  closeRelaySession();
  void resetAnonIdentity();

  return { keysRemoved };
}

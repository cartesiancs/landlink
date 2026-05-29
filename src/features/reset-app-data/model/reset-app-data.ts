import { _resetDebugModeStore, setDebugMode } from "@/entities/debug-mode";
import { clearAllMessages } from "@/entities/landlink-device";
import {
  _resetRegisteredDevicesStore,
  clearRegisteredDevices,
} from "@/entities/registered-device";

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
  // Persisted channel-chat history lives in IndexedDB, not localStorage,
  // so the prefix sweep above doesn't reach it. Fire-and-forget: any IDB
  // error is warn-logged inside clearAllMessages and must not block the
  // synchronous reset (the button caller already toasts success).
  void clearAllMessages();

  return { keysRemoved };
}

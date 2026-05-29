import { Capacitor } from "@capacitor/core";
import { LocalNotifications } from "@capacitor/local-notifications";

import { isAppActive } from "./app-state";

type IncomingChat = {
  senderNodeId: string;
  text: string;
  pktId: number | null;
};

const NOTIFICATION_ID_MASK = 0x7fffffff;
const TEXT_PREVIEW_MAX = 80;

let permissionGranted: boolean | null = null;

export async function requestNotificationPermission(): Promise<boolean> {
  if (!Capacitor.isNativePlatform()) {
    permissionGranted = false;
    return false;
  }

  try {
    const current = await LocalNotifications.checkPermissions();
    if (current.display === "granted") {
      permissionGranted = true;
      return true;
    }
    if (current.display === "denied") {
      permissionGranted = false;
      return false;
    }
    const result = await LocalNotifications.requestPermissions();
    permissionGranted = result.display === "granted";
    return permissionGranted;
  } catch (err) {
    console.warn("[notifications] permission request failed", err);
    permissionGranted = false;
    return false;
  }
}

function shortenSender(nodeId: string): string {
  if (nodeId.length <= 8) return nodeId;
  return nodeId.slice(0, 8);
}

function previewText(text: string): string {
  if (text.length <= TEXT_PREVIEW_MAX) return text;
  return text.slice(0, TEXT_PREVIEW_MAX) + "...";
}

function notificationIdFor(pktId: number | null): number {
  if (pktId === null) {
    return Date.now() & NOTIFICATION_ID_MASK;
  }
  return pktId & NOTIFICATION_ID_MASK;
}

export function notifyIncomingChat(input: IncomingChat): void {
  if (!Capacitor.isNativePlatform()) return;
  if (isAppActive()) return;
  if (permissionGranted === false) return;

  const id = notificationIdFor(input.pktId);
  const title = shortenSender(input.senderNodeId);
  const body = previewText(input.text);
  const threadIdentifier = "landlink:" + input.senderNodeId;

  void LocalNotifications.schedule({
    notifications: [
      {
        id,
        title,
        body,
        threadIdentifier,
      },
    ],
  }).catch((err: unknown) => {
    console.warn("[notifications] schedule failed", err);
  });
}

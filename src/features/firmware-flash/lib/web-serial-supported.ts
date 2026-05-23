import { Capacitor } from "@capacitor/core";

export function isWebSerialSupported(): boolean {
  if (Capacitor.isNativePlatform()) return false;
  return typeof navigator !== "undefined" && "serial" in navigator;
}

import { Capacitor } from "@capacitor/core";
import { Haptics, ImpactStyle } from "@capacitor/haptics";

export function hapticTick(): void {
  if (Capacitor.isNativePlatform()) {
    void Haptics.impact({ style: ImpactStyle.Light });
    return;
  }

  if (typeof navigator.vibrate === "function") {
    navigator.vibrate(10);
  }
}

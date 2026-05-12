import { Capacitor } from "@capacitor/core";
import { useLocation } from "react-router-dom";

import { useRegisteredDevices } from "@/entities/registered-device";
import { ROUTES } from "@/shared/config";

const IS_NATIVE_APP = Capacitor.isNativePlatform();

const NAV_ROUTES: ReadonlySet<string> = new Set([
  ROUTES.home,
  ROUTES.lists,
  ROUTES.settings,
]);

export function useBottomNavVisible(): boolean {
  const { pathname } = useLocation();
  const devices = useRegisteredDevices();
  return IS_NATIVE_APP && devices.length > 0 && NAV_ROUTES.has(pathname);
}

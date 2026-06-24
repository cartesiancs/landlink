import { Capacitor } from "@capacitor/core";
import { useLocation } from "react-router-dom";

import { ROUTES } from "@/shared/config";

const IS_NATIVE_APP = Capacitor.isNativePlatform();

const NAV_ROUTES: ReadonlySet<string> = new Set([
  ROUTES.home,
  ROUTES.lists,
  ROUTES.map,
  ROUTES.channels,
  ROUTES.settings,
]);

export function useBottomNavVisible(): boolean {
  const { pathname } = useLocation();
  return IS_NATIVE_APP && NAV_ROUTES.has(pathname);
}

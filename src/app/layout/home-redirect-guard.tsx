import { Navigate, useLocation } from "react-router-dom";

import { useRegisteredDevices } from "@/entities/registered-device";
import { ROUTES } from "@/shared/config";
import { HomePage } from "@/pages/home";

function isExplicitNav(state: unknown): boolean {
  if (typeof state !== "object" || state === null) return false;
  if (!("fromNav" in state)) return false;
  return (state as { fromNav?: unknown }).fromNav === true;
}

export function HomeOrListsRedirect() {
  const devices = useRegisteredDevices();
  const location = useLocation();
  if (!isExplicitNav(location.state) && devices.length > 0) {
    return <Navigate to={ROUTES.lists} replace />;
  }
  return <HomePage />;
}

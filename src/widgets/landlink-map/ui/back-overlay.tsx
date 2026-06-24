import { Capacitor } from "@capacitor/core";
import { ChevronLeft } from "lucide-react";
import { useLocation, useNavigate } from "react-router-dom";

import { ROUTES } from "@/shared/config";
import { cn, hapticTick } from "@/shared/lib";

const IS_NATIVE_APP = Capacitor.isNativePlatform();

// Floating back affordance for the map page. The fullscreen map has no
// PageHeader, so on web (where BottomNav is hidden) the user otherwise has
// no way to leave the page. On native, BottomNav handles tab switching so
// this overlay would just clutter the corner.
export function BackOverlay() {
  const navigate = useNavigate();
  const location = useLocation();
  if (IS_NATIVE_APP) return null;

  const handle = () => {
    hapticTick();
    if (location.key === "default") {
      void navigate(ROUTES.home, { viewTransition: true });
      return;
    }
    navigate(-1);
  };

  return (
    <button
      type="button"
      onClick={handle}
      aria-label="Go back"
      className={cn(
        "absolute left-3 top-[max(env(safe-area-inset-top),0px)] z-[1000] mt-3",
        "flex h-10 w-10 items-center justify-center",
        "text-foreground",
        "transition-colors hover:bg-card",
      )}
    >
      <ChevronLeft className="h-4 w-4" aria-hidden />
    </button>
  );
}

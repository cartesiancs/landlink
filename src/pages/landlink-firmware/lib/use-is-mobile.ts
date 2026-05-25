import { useSyncExternalStore } from "react";
import { Capacitor } from "@capacitor/core";

const MOBILE_UA =
  /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini|Mobile|Tablet/i;
const NARROW_MEDIA = "(max-width: 1023px)";
const STATIC_MOBILE =
  Capacitor.isNativePlatform() ||
  (typeof navigator !== "undefined" && MOBILE_UA.test(navigator.userAgent));

function subscribeNarrow(onChange: () => void): () => void {
  const mql = window.matchMedia(NARROW_MEDIA);
  mql.addEventListener("change", onChange);
  return () => {
    mql.removeEventListener("change", onChange);
  };
}

function readNarrow(): boolean {
  return window.matchMedia(NARROW_MEDIA).matches;
}

function readNarrowServer(): boolean {
  return false;
}

export function useIsMobile(): boolean {
  const narrow = useSyncExternalStore(
    subscribeNarrow,
    readNarrow,
    readNarrowServer,
  );
  return STATIC_MOBILE || narrow;
}

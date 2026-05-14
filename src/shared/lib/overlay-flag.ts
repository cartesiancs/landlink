import { useEffect } from "react";

// WHY: tracks how many overlays (Sheet/Drawer/Dialog) are currently open so a
// single global flag on <html> reflects the union. `installViewTransitionFlag`
// reads this flag when a view transition starts and disables the headers'
// `view-transition-name` for that transition — that keeps AppHeader and
// BottomNavBar inside the root snapshot so they don't pop above the
// closing overlay's backdrop while the route changes.

const FLAG_ATTR = "data-overlay-open";

let openCount = 0;

export function useOverlayOpenFlag(open: boolean): void {
  useEffect(() => {
    if (!open) return;
    openCount += 1;
    if (openCount === 1) {
      document.documentElement.setAttribute(FLAG_ATTR, "");
    }
    return () => {
      openCount -= 1;
      if (openCount === 0) {
        document.documentElement.removeAttribute(FLAG_ATTR);
      }
    };
  }, [open]);
}

export function isOverlayOpen(): boolean {
  return document.documentElement.hasAttribute(FLAG_ATTR);
}

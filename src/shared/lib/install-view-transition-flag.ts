import { isOverlayOpen } from "./overlay-flag";

// WHY: `view-transition-name` hoists an element out of the `root` snapshot
// into its own group, which is painted *above* the root during a transition.
// When a route change is started from inside an overlay (e.g. the navigation
// sidebar's sheet), the closing overlay belongs to root, so a pinned
// AppHeader would suddenly stack on top of the overlay's backdrop. We wrap
// `document.startViewTransition` so that — whenever a transition begins with
// an overlay open — a `data-suppress-pin` flag is set on <html> for the
// duration of the transition. CSS gates the headers' `view-transition-name`
// on the absence of that flag, so they stay inside the root snapshot and
// cross-fade with the rest of the page.

const SUPPRESS_ATTR = "data-suppress-pin";

export function installViewTransitionFlag(): void {
  if (typeof document === "undefined") return;
  if (typeof document.startViewTransition !== "function") return;

  type StartFn = typeof document.startViewTransition;
  const original = document.startViewTransition.bind(document);
  const root = document.documentElement;

  const patched = ((...args: Parameters<StartFn>) => {
    const suppress = isOverlayOpen();
    if (suppress) {
      root.setAttribute(SUPPRESS_ATTR, "");
    }
    const transition = original(...args);
    if (suppress) {
      const cleanup = () => {
        root.removeAttribute(SUPPRESS_ATTR);
      };
      transition.finished.then(cleanup, cleanup);
    }
    return transition;
  }) as StartFn;

  document.startViewTransition = patched;
}

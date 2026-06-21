import { Capacitor } from "@capacitor/core";
import posthog from "posthog-js";
import { PostHogProvider } from "@posthog/react";
import { createRoot } from "react-dom/client";

import { App } from "@/app";
import { installViewTransitionFlag } from "@/shared/lib";

installViewTransitionFlag();

// WHY: Capacitor serves the WebView from capacitor://localhost (iOS) and
// http://localhost (Android), so a plain hostname check would suppress
// PostHog on every native build. Treat native platforms as non-localhost.
const isNative = Capacitor.isNativePlatform();
const isLocalhost =
  !isNative &&
  (window.location.hostname === "localhost" ||
    window.location.hostname === "127.0.0.1" ||
    window.location.hostname === "[::1]");

// WHY: native builds use a hash router, so the URL PostHog sees is
// `capacitor://localhost#/some/path`. PostHog parses $pathname from
// window.location.pathname (always "/") and $host as "localhost", which
// collapses every screen into the same row in insights. Rewrite the URL
// at capture time so pathname/host reflect the real route for every
// event type (pageview, autocapture, custom).
function rewriteNativeUrl(raw: unknown): string | null {
  if (typeof raw !== "string") return null;
  if (!raw.startsWith("capacitor://localhost")) return null;
  const hashIndex = raw.indexOf("#");
  const after = hashIndex >= 0 ? raw.slice(hashIndex + 1) : "";
  const path = after.startsWith("/") ? after : `/${after}`;
  return `capacitor://landlink${path}`;
}

if (!isLocalhost) {
  posthog.init(import.meta.env.VITE_PUBLIC_POSTHOG_PROJECT_TOKEN, {
    api_host: import.meta.env.VITE_PUBLIC_POSTHOG_HOST,
    defaults: "2026-01-30",
    before_send: (event) => {
      if (!event?.properties) return event;
      const fixed = rewriteNativeUrl(event.properties["$current_url"]);
      if (fixed) {
        const url = new URL(fixed);
        event.properties["$current_url"] = fixed;
        event.properties["$pathname"] = url.pathname;
        event.properties["$host"] = url.host;
      }
      return event;
    },
  });
}

const rootElement = document.getElementById("root");
if (!rootElement) {
  throw new Error("Root element '#root' not found in index.html");
}

createRoot(rootElement).render(
  <PostHogProvider client={posthog}>
    <App />
  </PostHogProvider>,
);

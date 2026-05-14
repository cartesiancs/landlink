import posthog from "posthog-js";
import { PostHogProvider } from "@posthog/react";
import { createRoot } from "react-dom/client";

import { App } from "@/app";
import { installViewTransitionFlag } from "@/shared/lib";

installViewTransitionFlag();

const isLocalhost =
  window.location.hostname === "localhost" ||
  window.location.hostname === "127.0.0.1" ||
  window.location.hostname === "[::1]";

if (!isLocalhost) {
  posthog.init(import.meta.env.VITE_PUBLIC_POSTHOG_PROJECT_TOKEN, {
    api_host: import.meta.env.VITE_PUBLIC_POSTHOG_HOST,
    defaults: "2026-01-30",
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

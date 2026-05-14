import posthog from "posthog-js";
import { PostHogProvider } from "@posthog/react";
import { createRoot } from "react-dom/client";

import { App } from "@/app";

posthog.init(import.meta.env.VITE_PUBLIC_POSTHOG_PROJECT_TOKEN, {
  api_host: import.meta.env.VITE_PUBLIC_POSTHOG_HOST,
  defaults: "2026-01-30",
});

const rootElement = document.getElementById("root");
if (!rootElement) {
  throw new Error("Root element '#root' not found in index.html");
}

createRoot(rootElement).render(
  <PostHogProvider client={posthog}>
    <App />
  </PostHogProvider>,
);

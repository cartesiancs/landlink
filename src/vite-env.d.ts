/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_PUBLIC_POSTHOG_PROJECT_TOKEN: string;
  readonly VITE_PUBLIC_POSTHOG_HOST: string;
  // Base URL of the optional anonymous relay (e.g. wss://relay.landlink.sh).
  // Empty/undefined disables all remote connectivity — the app stays BLE-only.
  readonly VITE_LANDLINK_RELAY_URL?: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}

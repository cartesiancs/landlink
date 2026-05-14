<wizard-report>
# PostHog post-wizard report

The wizard has completed a deep integration of PostHog analytics into the Vision app. The following changes were made:

- **`src/main.tsx`** — Added `posthog.init` with environment variables and wrapped the app in `PostHogProvider` from `@posthog/react`.
- **`src/vite-env.d.ts`** — Created Vite environment type declarations for `VITE_PUBLIC_POSTHOG_PROJECT_TOKEN` and `VITE_PUBLIC_POSTHOG_HOST`.
- **`src/app/router/app-router.tsx`** — Added a `PostHogPageTracker` layout route component that captures `$pageview` on every route change using `useLocation`.
- **`src/features/bluetooth-pairing/model/use-bluetooth-pairing.ts`** — Added capture calls for all four Bluetooth pairing outcomes (started, succeeded, cancelled, failed) with relevant properties.
- **`src/pages/device-dashboard/ui/device-dashboard-page.tsx`** — Added `device_removed` capture with device ID and name when a user confirms device removal.
- **`src/features/send-mesh-message/ui/send-mesh-form.tsx`** — Added `mesh_message_sent` capture with byte length when a message is successfully delivered.
- **`src/features/reset-app-data/ui/reset-app-data-button.tsx`** — Added `app_data_reset` capture when the user confirms a full data wipe.
- **`src/features/register-mock-device/ui/register-mock-device-button.tsx`** — Added `mock_device_registered` capture with device name when a debug mock device is added.
- **`.env`** — Created with `VITE_PUBLIC_POSTHOG_PROJECT_TOKEN` and `VITE_PUBLIC_POSTHOG_HOST`.

## Events

| Event | Description | File |
|-------|-------------|------|
| `bluetooth_pairing_started` | User initiates a Bluetooth pairing attempt | `src/features/bluetooth-pairing/model/use-bluetooth-pairing.ts` |
| `bluetooth_pairing_succeeded` | Bluetooth pairing completed successfully | `src/features/bluetooth-pairing/model/use-bluetooth-pairing.ts` |
| `bluetooth_pairing_cancelled` | User cancelled the Bluetooth pairing dialog | `src/features/bluetooth-pairing/model/use-bluetooth-pairing.ts` |
| `bluetooth_pairing_failed` | Bluetooth pairing failed due to an error | `src/features/bluetooth-pairing/model/use-bluetooth-pairing.ts` |
| `device_removed` | User confirmed removal of a registered device | `src/pages/device-dashboard/ui/device-dashboard-page.tsx` |
| `mesh_message_sent` | User successfully sent a mesh message | `src/features/send-mesh-message/ui/send-mesh-form.tsx` |
| `app_data_reset` | User confirmed reset of all app data | `src/features/reset-app-data/ui/reset-app-data-button.tsx` |
| `mock_device_registered` | User registered a mock device (debug) | `src/features/register-mock-device/ui/register-mock-device-button.tsx` |

## Next steps

We've built some insights and a dashboard for you to keep an eye on user behavior, based on the events we just instrumented:

- [Analytics basics dashboard](/dashboard/1583287)
- [Bluetooth Pairing Funnel](/insights/Ol08LtSf) — Conversion from pairing started to pairing succeeded
- [Bluetooth Pairing Outcomes](/insights/27sCe0gM) — Succeeded, cancelled, and failed trends over time
- [Mesh Messages Sent](/insights/LZ4WpgDm) — Volume of mesh messages sent to connected devices
- [Device Removals](/insights/OoXzrTVz) — Device removal trend (churn signal)
- [App Data Resets](/insights/Zz1C7v1L) — App data reset count (strongest churn indicator)

### Agent skill

We've left an agent skill folder in your project. You can use this context for further agent development when using Claude Code. This will help ensure the model provides the most up-to-date approaches for integrating PostHog.

</wizard-report>

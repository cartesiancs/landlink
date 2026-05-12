import { useDebugMode } from "@/entities/debug-mode";
import { RegisterMockDeviceButton } from "@/features/register-mock-device";
import { DebugModeToggle } from "@/features/toggle-debug-mode";
import { ROUTES } from "@/shared/config";
import { PageHeader } from "@/widgets/page-header";

export function SettingsDebugPage() {
  const debugEnabled = useDebugMode();

  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <PageHeader
        title="Debug mode"
        fallback={ROUTES.settings}
        backLabel="Back to Settings"
      />

      <section className="flex flex-col gap-3 px-4 pt-2 pb-6">
        <p className="text-sm leading-relaxed text-muted-foreground">
          Reveals developer tools, including mock device registration. Mock
          devices register through the same path as real Landlinks but stay
          disabled, so they cannot interfere with live BLE connections.
        </p>
        <div className="rounded-md border border-border px-4 py-3">
          <DebugModeToggle />
        </div>
        {debugEnabled && (
          <div className="flex flex-col gap-2 rounded-md border border-dashed border-border p-3">
            <p className="text-xs text-muted-foreground">
              Each tap registers one disabled mock device.
            </p>
            <RegisterMockDeviceButton />
          </div>
        )}
      </section>
    </main>
  );
}

import { useState } from "react";

import { useDebugMode } from "@/entities/debug-mode";
import { ResetAppDataButton } from "@/features/reset-app-data";
import { RegisterMockDeviceButton } from "@/features/register-mock-device";
import { ThemeToggle } from "@/features/toggle-theme";
import { DebugModeToggle } from "@/features/toggle-debug-mode";
import { AppHeader } from "@/widgets/app-header";
import { NavigationSidebar } from "@/widgets/navigation-sidebar";
import { SupportDrawer } from "@/widgets/support-drawer";

export function SettingsPage() {
  const [sidebarOpen, setSidebarOpen] = useState(false);
  const [supportOpen, setSupportOpen] = useState(false);
  const debugEnabled = useDebugMode();

  return (
    <div className="mx-auto flex h-dvh w-full max-w-[430px] flex-col bg-background">
      <AppHeader
        onMenuOpen={() => {
          setSidebarOpen(true);
        }}
        onSupportOpen={() => {
          setSupportOpen(true);
        }}
      />
      <div className="px-4 pt-1 pb-3">
        <h1 className="text-base font-medium">Settings</h1>
      </div>
      <main className="min-h-0 flex-1 overflow-y-auto px-4 pb-[max(env(safe-area-inset-bottom),1.5rem)]">
        <section className="flex flex-col gap-2 pt-2 pb-6">
          <h2 className="px-1 text-xs font-medium uppercase tracking-wide text-muted-foreground">
            Appearance
          </h2>
          <div className="flex items-center justify-between rounded-md px-1 py-2">
            <span className="text-sm font-medium">Theme</span>
            <ThemeToggle />
          </div>
        </section>

        <section className="flex flex-col gap-2 border-t border-border pt-4 pb-6">
          <h2 className="px-1 text-xs font-medium uppercase tracking-wide text-muted-foreground">
            Developer
          </h2>
          <DebugModeToggle />
          {debugEnabled && (
            <div className="mt-2 flex flex-col gap-2 rounded-md border border-dashed border-border p-3">
              <p className="text-xs text-muted-foreground">
                Mock devices register through the same path as real Landlinks
                but stay disabled, so they cannot interfere with live BLE
                connections.
              </p>
              <RegisterMockDeviceButton />
            </div>
          )}
        </section>

        <section className="flex flex-col gap-2 border-t border-border pt-4 pb-6">
          <h2 className="px-1 text-xs font-medium uppercase tracking-wide text-muted-foreground">
            Danger zone
          </h2>
          <ResetAppDataButton />
        </section>
      </main>
      <NavigationSidebar open={sidebarOpen} onOpenChange={setSidebarOpen} />
      <SupportDrawer open={supportOpen} onOpenChange={setSupportOpen} />
    </div>
  );
}

import { useState } from "react";
import { ChevronRight } from "lucide-react";
import { Link } from "react-router-dom";

import { ROUTES } from "@/shared/config";
import { cn } from "@/shared/lib";
import { AppHeader } from "@/widgets/app-header";
import { BottomNavBar, useBottomNavVisible } from "@/widgets/bottom-nav-bar";
import { NavigationSidebar } from "@/widgets/navigation-sidebar";
import { SupportDrawer } from "@/widgets/support-drawer";

type SettingEntry = {
  id: string;
  label: string;
  to: string;
};

const ENTRIES: readonly SettingEntry[] = [
  { id: "theme", label: "Theme", to: ROUTES.settingsTheme },
  { id: "protocol", label: "Meshtastic compatibility", to: ROUTES.settingsProtocol },
  { id: "region", label: "Region", to: ROUTES.settingsRegion },
  { id: "debug", label: "Debug mode", to: ROUTES.settingsDebug },
  { id: "reset", label: "Reset all data", to: ROUTES.settingsReset },
];

export function SettingsPage() {
  const [sidebarOpen, setSidebarOpen] = useState(false);
  const [supportOpen, setSupportOpen] = useState(false);
  const navVisible = useBottomNavVisible();

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
      <main
        className={cn(
          "min-h-0 flex-1 overflow-y-auto px-4",
          navVisible
            ? "pb-[calc(max(env(safe-area-inset-bottom),0.75rem)+4.5rem)]"
            : "pb-[max(env(safe-area-inset-bottom),1.5rem)]",
        )}
      >
        <section
          aria-label="Settings sections"
          className="overflow-hidden rounded-2xl border border-border bg-card"
        >
          <ul className="divide-y divide-border">
            {ENTRIES.map((entry) => (
              <li key={entry.id}>
                <Link
                  to={entry.to}
                  viewTransition
                  className="flex items-center justify-between px-4 py-4 text-sm font-medium transition-colors hover:bg-muted"
                >
                  <span>{entry.label}</span>
                  <ChevronRight
                    className="size-4 text-muted-foreground"
                    aria-hidden="true"
                  />
                </Link>
              </li>
            ))}
          </ul>
        </section>
      </main>
      <NavigationSidebar open={sidebarOpen} onOpenChange={setSidebarOpen} />
      <SupportDrawer open={supportOpen} onOpenChange={setSupportOpen} />
      <BottomNavBar />
    </div>
  );
}

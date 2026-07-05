import { Capacitor } from "@capacitor/core";
import { useState } from "react";

import { AppHeader } from "@/widgets/app-header";
import { BottomNavBar } from "@/widgets/bottom-nav-bar";
import { LandlinkMap } from "@/widgets/landlink-map";
import { NavigationSidebar } from "@/widgets/navigation-sidebar";
import { SupportDrawer } from "@/widgets/support-drawer";

const IS_NATIVE_APP = Capacitor.isNativePlatform();

export function MapPage() {
  const [sidebarOpen, setSidebarOpen] = useState(false);
  const [supportOpen, setSupportOpen] = useState(false);

  return (
    <main className="relative flex h-dvh w-full flex-col bg-background">
      {IS_NATIVE_APP && (
        <AppHeader
          onMenuOpen={() => {
            setSidebarOpen(true);
          }}
          onSupportOpen={() => {
            setSupportOpen(true);
          }}
        />
      )}
      <div className="relative min-h-0 flex-1">
        <LandlinkMap />
      </div>
      <NavigationSidebar open={sidebarOpen} onOpenChange={setSidebarOpen} />
      <SupportDrawer open={supportOpen} onOpenChange={setSupportOpen} />
      <BottomNavBar />
    </main>
  );
}

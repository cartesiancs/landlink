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
      {IS_NATIVE_APP ? (
        <AppHeader
          onMenuOpen={() => {
            setSidebarOpen(true);
          }}
          onSupportOpen={() => {
            setSupportOpen(true);
          }}
        />
      ) : (
        // Web: same header as the other pages, floating above the
        // fullscreen map. z-1100 keeps it over Leaflet panes (up to z-700)
        // and the map control overlays (z-1000). Back lives in the header
        // since the map has no other exit on web.
        <div className="absolute inset-x-0 top-0 z-1100 mx-auto w-full max-w-[430px]">
          <AppHeader
            showBack
            onSupportOpen={() => {
              setSupportOpen(true);
            }}
          />
        </div>
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

import { useState } from "react";

import { Button } from "@/shared/ui";
import { hapticTick } from "@/shared/lib";
import { AppHeader } from "@/widgets/app-header";
import { NavigationSidebar } from "@/widgets/navigation-sidebar";
import { SupportDrawer } from "@/widgets/support-drawer";
import { HomeHeroCarousel } from "@/widgets/home-hero-carousel";
import { HomeLinkList } from "@/widgets/home-link-list";

export function HomePage() {
  const [sidebarOpen, setSidebarOpen] = useState(false);
  const [supportOpen, setSupportOpen] = useState(false);

  return (
    <div className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <AppHeader
        onMenuOpen={() => {
          setSidebarOpen(true);
        }}
        onSupportOpen={() => {
          setSupportOpen(true);
        }}
      />

      <main className="flex-1 overflow-y-auto pb-24">
        <section className="px-4 pt-6 pb-4">
          <h1 className="font-display text-3xl font-normal leading-tight tracking-tight">
            Alternatives <br /> to Starlink
          </h1>
        </section>

        <section className="px-4 pb-6">
          <HomeHeroCarousel />
        </section>

        <section className="px-4 pb-8">
          <HomeLinkList />
        </section>
      </main>

      <div className="pointer-events-none fixed inset-x-0 bottom-0 z-20 flex justify-center">
        <div className="pointer-events-auto w-full max-w-[430px] bg-background/90 px-4 pt-3 pb-[max(env(safe-area-inset-bottom),12px)] backdrop-blur supports-backdrop-filter:bg-background/70">
          <Button
            size="lg"
            className="h-12 w-full text-base"
            onClick={hapticTick}
          >
            Get started
          </Button>
        </div>
      </div>

      <NavigationSidebar open={sidebarOpen} onOpenChange={setSidebarOpen} />
      <SupportDrawer open={supportOpen} onOpenChange={setSupportOpen} />
    </div>
  );
}

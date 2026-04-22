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
    <div className="mx-auto flex h-dvh w-full max-w-[430px] flex-col bg-background">
      <AppHeader
        onMenuOpen={() => {
          setSidebarOpen(true);
        }}
        onSupportOpen={() => {
          setSupportOpen(true);
        }}
      />

      <main className="flex-1 min-h-0 snap-y snap-mandatory overflow-y-auto overscroll-contain">
        <section className="flex h-full shrink-0 snap-start snap-always flex-col overflow-hidden">
          <div className="shrink-0 px-4 pt-4 pb-3">
            <h1 className="font-display text-3xl font-normal leading-tight tracking-tight">
              Alternatives <br /> to Starlink
            </h1>
          </div>

          <div className="shrink-0 px-4 pb-4">
            <HomeHeroCarousel />
          </div>

          <div className="shrink-0 px-4 pb-4">
            <HomeLinkList />
          </div>
        </section>

        <section className="flex h-full shrink-0 snap-start snap-always items-center justify-center">
          <p className="text-sm text-muted-foreground">Page 2 — coming soon</p>
        </section>

        <section className="flex h-full shrink-0 snap-start snap-always items-center justify-center">
          <p className="text-sm text-muted-foreground">Page 3 — coming soon</p>
        </section>

        <section className="flex h-full shrink-0 snap-start snap-always items-center justify-center">
          <p className="text-sm text-muted-foreground">Page 4 — coming soon</p>
        </section>
      </main>

      <div className="bg-background/90 px-4 pt-3 pb-[max(env(safe-area-inset-bottom),12px)] backdrop-blur supports-backdrop-filter:bg-background/70">
        <Button
          size="lg"
          className="h-12 w-full text-base"
          onClick={hapticTick}
        >
          Get started
        </Button>
      </div>

      <NavigationSidebar open={sidebarOpen} onOpenChange={setSidebarOpen} />
      <SupportDrawer open={supportOpen} onOpenChange={setSupportOpen} />
    </div>
  );
}

import { useState } from "react";

import { Button } from "@/shared/ui";
import { hapticTick } from "@/shared/lib";
import { AppHeader } from "@/widgets/app-header";
import { NavigationSidebar } from "@/widgets/navigation-sidebar";
import { SupportDrawer } from "@/widgets/support-drawer";
import { HomeHeroCarousel } from "@/widgets/home-hero-carousel";
import { HomeLinkList } from "@/widgets/home-link-list";
import { HomeStep } from "@/widgets/home-step";

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

      <main className="flex-1 min-h-0 snap-y snap-mandatory overflow-y-auto overscroll-contain [scrollbar-width:none] [&::-webkit-scrollbar]:hidden">
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

        <section className="h-full shrink-0 snap-start snap-always overflow-hidden">
          <HomeStep
            step={1}
            title="Buy a Drone"
            description="Powered by cartesiancs' technology, you can easily set up a drone with a single connection that reaches up to 50km in range."
            mediaLabel="Drone Image"
          />
        </section>

        <section className="h-full shrink-0 snap-start snap-always overflow-hidden">
          <HomeStep
            step={2}
            title="Connect Your Drone"
            description="Get connected in under a minute. No hassle, no complicated setup."
            mediaLabel="Drone connect animation"
          />
        </section>

        <section className="h-full shrink-0 snap-start snap-always overflow-hidden">
          <HomeStep
            step={3}
            title="Ground Station Setup"
            description="Build a Mesh network with simple, plug-and-play modules. Get started right away with a ground station that comes with a camera, microphone, speaker, and communication module built in."
            mediaLabel="Animation Image"
          />
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

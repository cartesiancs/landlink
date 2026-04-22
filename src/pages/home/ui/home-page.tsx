import { HomeHeroCarousel } from "@/widgets/home-hero-carousel";
import { HomeLinkList } from "@/widgets/home-link-list";
import { HomeStep } from "@/widgets/home-step";

export function HomePage() {
  return (
    <main className="h-full snap-y snap-mandatory overflow-y-auto overscroll-contain [scrollbar-width:none] [&::-webkit-scrollbar]:hidden">
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
  );
}

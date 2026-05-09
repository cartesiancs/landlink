import { ROUTES } from "@/shared/config";
import { BackButton } from "@/shared/ui";

export function AboutPage() {
  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <header className="sticky top-0 z-10 flex items-center gap-2 bg-background/90 px-4 ps-1 pt-[max(env(safe-area-inset-top),0.75rem)] pb-3 backdrop-blur supports-backdrop-filter:bg-background/70">
        <BackButton fallback={ROUTES.home} aria-label="Back to Home" />
        <h1 className="text-base font-medium">About</h1>
      </header>

      <section className="px-4 pt-2 pb-6">
        <h2 className="font-display text-3xl leading-tight tracking-tight">
          Connectivity
          <br />
          without satellites
        </h2>
        <p className="mt-3 text-sm leading-relaxed text-muted-foreground">
          Landlink is a drone-native connectivity platform from cartesiancs. We
          replace satellite dependence with a fleet of autonomous relays that
          carry your signal further, faster, and on your terms.
        </p>
      </section>

      <section className="px-4 pb-8">
        <h3 className="font-display text-2xl leading-tight tracking-tight">
          Drone-powered mesh
        </h3>
        <p className="mt-2 text-sm leading-relaxed text-muted-foreground">
          Autonomous drones extend a self-healing mesh network up to 50km from a
          single ground station.
        </p>
      </section>

      <section className="px-4 pb-8">
        <h3 className="font-display text-2xl leading-tight tracking-tight">
          Fly from your phone
        </h3>
        <p className="mt-2 text-sm leading-relaxed text-muted-foreground">
          The mesh is a two-way link, so the same network that carries your
          messages also steers the drones above it. Reposition any node, set a
          new patrol path, or recall the fleet straight from your smartphone,
          with no ground controller required.
        </p>
      </section>

      <section className="px-4 pb-8">
        <h3 className="font-display text-2xl leading-tight tracking-tight">
          Starlink alternative
        </h3>
        <p className="mt-2 text-sm leading-relaxed text-muted-foreground">
          Stay online in places where satellite terminals struggle, like
          forests, canyons, coastal fog, and dense foliage.
        </p>
      </section>

      <section className="px-4 pb-8">
        <h3 className="font-display text-2xl leading-tight tracking-tight">
          Plug-and-play hardware
        </h3>
        <p className="mt-2 text-sm leading-relaxed text-muted-foreground">
          Every kit ships with the radio module pre-configured and is ready to
          relay text payloads out of the box. Camera, microphone, and speaker
          modules are sold separately and can be added whenever you need richer
          media.
        </p>
      </section>

      <section className="px-4 pb-8">
        <h3 className="font-display text-2xl leading-tight tracking-tight">
          Built for the field
        </h3>
        <p className="mt-2 text-sm leading-relaxed text-muted-foreground">
          Ruggedized for outdoor operations with encrypted links, rotating
          battery swaps, and zero-config failover.
        </p>
      </section>

      <section className="px-4 pb-10">
        <h3 className="font-display text-2xl leading-tight tracking-tight">
          Our mission
        </h3>
        <p className="mt-2 text-sm leading-relaxed text-muted-foreground">
          We believe the ground should not be a dead zone. Landlink brings
          always-on connectivity to the places satellites cannot reach, powered
          by airborne infrastructure that you own and operate.
        </p>
      </section>

      <footer className="mt-auto border-t border-border px-4 pt-6 pb-[max(env(safe-area-inset-bottom),1.5rem)] text-xs text-muted-foreground">
        Made by{" "}
        <a
          href="https://cartesiancs.com"
          target="_blank"
          rel="noreferrer noopener"
          className="underline underline-offset-2 hover:text-foreground"
        >
          cartesiancs
        </a>
        .
      </footer>
    </main>
  );
}

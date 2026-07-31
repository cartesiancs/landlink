import { ROUTES } from "@/shared/config";
import { PageHeader } from "@/widgets/page-header";
import { Reveal } from "./reveal";

export function AboutPage() {
  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <PageHeader
        title="About"
        fallback={ROUTES.home}
        backLabel="Back to Home"
      />

      <section className="px-4 pt-4">
        <Reveal order={0}>
          <img
            src="/banner.webp"
            alt="Landlink drones relaying a mesh network over open terrain"
            className="w-full object-cover rounded-2xl"
          />
        </Reveal>
      </section>

      <section className="px-4 pt-8 pb-6">
        <Reveal order={1}>
          <h2 className="font-display text-3xl leading-tight tracking-tight">
            Off grid
            <br />
            Connectivity
          </h2>
        </Reveal>
        <Reveal order={2}>
          <p className="mt-3 text-sm leading-relaxed text-muted-foreground">
            Landlink is a drone-native connectivity platform from cartesiancs.
            A fleet of autonomous relays weaves a self-healing mesh that
            carries your signal further, faster, and on your terms.
          </p>
        </Reveal>
      </section>

      <section className="px-4 pb-8">
        <Reveal order={3}>
          <h3 className="font-display text-2xl leading-tight tracking-tight">
            Drone-powered mesh
          </h3>
        </Reveal>
        <Reveal order={4}>
          <p className="mt-2 text-sm leading-relaxed text-muted-foreground">
            Autonomous drones extend a self-healing mesh network up to 50km
            from a single ground station.
          </p>
        </Reveal>
      </section>

      <section className="px-4 pb-8">
        <Reveal order={5}>
          <h3 className="font-display text-2xl leading-tight tracking-tight">
            Fly from your phone
          </h3>
        </Reveal>
        <Reveal order={6}>
          <p className="mt-2 text-sm leading-relaxed text-muted-foreground">
            The mesh is a two-way link, so the same network that carries your
            messages also steers the drones above it. Reposition any node, set
            a new patrol path, or recall the fleet straight from your
            smartphone, with no ground controller required.
          </p>
        </Reveal>
      </section>

      <section className="px-4 pb-8">
        <Reveal order={7}>
          <h3 className="font-display text-2xl leading-tight tracking-tight">
            Coverage off the grid
          </h3>
        </Reveal>
        <Reveal order={8}>
          <p className="mt-2 text-sm leading-relaxed text-muted-foreground">
            Stay online where existing networks fall short, from forests and
            canyons to coastal fog and dense foliage.
          </p>
        </Reveal>
      </section>

      <section className="px-4 pb-8">
        <Reveal order={9}>
          <h3 className="font-display text-2xl leading-tight tracking-tight">
            Plug-and-play hardware
          </h3>
        </Reveal>
        <Reveal order={10}>
          <p className="mt-2 text-sm leading-relaxed text-muted-foreground">
            Every kit ships with the radio module pre-configured and is ready
            to relay text payloads out of the box. Camera, microphone, and
            speaker modules are sold separately and can be added whenever you
            need richer media.
          </p>
        </Reveal>
      </section>

      <section className="px-4 pb-8">
        <Reveal order={11}>
          <h3 className="font-display text-2xl leading-tight tracking-tight">
            Built for the field
          </h3>
        </Reveal>
        <Reveal order={12}>
          <p className="mt-2 text-sm leading-relaxed text-muted-foreground">
            Ruggedized for outdoor operations with encrypted links, rotating
            battery swaps, and zero-config failover.
          </p>
        </Reveal>
      </section>

      <section className="px-4 pb-10">
        <Reveal order={13}>
          <h3 className="font-display text-2xl leading-tight tracking-tight">
            Our mission
          </h3>
        </Reveal>
        <Reveal order={14}>
          <p className="mt-2 text-sm leading-relaxed text-muted-foreground">
            We believe the ground should not be a dead zone. Landlink weaves a
            mesh across the places existing networks cannot reach, powered by
            airborne infrastructure that you own and operate.
          </p>
        </Reveal>
      </section>
    </main>
  );
}

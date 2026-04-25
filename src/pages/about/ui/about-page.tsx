import { ChevronLeft } from "lucide-react";
import { Link } from "react-router-dom";

import { ROUTES } from "@/shared/config";

type Highlight = {
  title: string;
  description: string;
};

const HIGHLIGHTS: readonly Highlight[] = [
  {
    title: "Drone-powered mesh",
    description:
      "Autonomous drones extend a self-healing mesh network up to 50km from a single ground station.",
  },
  {
    title: "Starlink alternative",
    description:
      "Stay online in places where satellite terminals struggle — forests, canyons, coastal fog, and dense foliage.",
  },
  {
    title: "Plug-and-play hardware",
    description:
      "Each kit ships with camera, microphone, speaker, and radio module pre-configured. Connect once and you're live.",
  },
  {
    title: "Built for the field",
    description:
      "Ruggedized for outdoor operations with encrypted links, rotating battery swaps, and zero-config failover.",
  },
];

export function AboutPage() {
  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <header className="sticky top-0 z-10 flex items-center gap-2 bg-background/90 px-4 ps-1 pt-[max(env(safe-area-inset-top),0.75rem)] pb-3 backdrop-blur supports-backdrop-filter:bg-background/70">
        <Link
          to={ROUTES.home}
          className="flex size-9 items-center justify-center rounded-md hover:bg-muted"
          aria-label="Back to Home"
        >
          <ChevronLeft className="size-5" aria-hidden="true" />
        </Link>
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

      <section className="flex flex-col gap-5 px-4 pb-8">
        {HIGHLIGHTS.map((item) => (
          <article key={item.title} className="flex flex-col gap-1">
            <h3 className="text-sm font-medium">{item.title}</h3>
            <p className="text-sm leading-relaxed text-muted-foreground">
              {item.description}
            </p>
          </article>
        ))}
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

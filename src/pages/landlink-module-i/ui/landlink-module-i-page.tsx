import { ChevronLeft } from "lucide-react";
import { Link } from "react-router-dom";

import { ROUTES } from "@/shared/config";
import { Button } from "@/shared/ui";

type Spec = {
  label: string;
  value: string;
};

const SPECS: readonly Spec[] = [
  { label: "Radio", value: "LoRa 868 / 915 MHz" },
  { label: "Range", value: "Up to 12 km LoS" },
  { label: "Mesh", value: "Self-healing relay" },
  { label: "Phone link", value: "Bluetooth LE" },
  { label: "Power", value: "USB-C · 5 W" },
  { label: "Weight", value: "180 g" },
];

export function LandlinkModuleIPage() {
  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <header className="sticky top-0 z-10 flex items-center gap-2 bg-background/90 px-4 ps-1 pt-[max(env(safe-area-inset-top),0.75rem)] pb-3 backdrop-blur supports-backdrop-filter:bg-background/70">
        <Link
          to={ROUTES.home}
          viewTransition
          className="flex size-9 items-center justify-center rounded-md hover:bg-muted"
          aria-label="Back to Home"
        >
          <ChevronLeft className="size-5" aria-hidden="true" />
        </Link>
        <h1 className="text-base font-medium">Landlink Module I</h1>
      </header>

      <section className="px-4 pt-2 pb-6">
        <div className="flex aspect-[4/3] w-full items-center justify-center overflow-hidden rounded-2xl border border-border bg-muted">
          <img
            src="/images/groundstation.webp"
            alt="Landlink Module I"
            className="h-[78%] w-auto object-contain"
          />
        </div>
        <div className="mt-5">
          <h2 className="mt-1 font-display text-3xl leading-tight tracking-tight">
            Landlink Module I
          </h2>
          <p className="mt-3 text-sm leading-relaxed text-muted-foreground">
            A pocket-sized LoRa mesh node. Your own personal base station that
            pairs with your phone over Bluetooth and relays every other module
            within range. The more you place, the denser your private network
            and the lower its latency.
          </p>
        </div>
      </section>

      <section className="px-4 pb-8">
        <dl className="overflow-hidden rounded-2xl border border-border bg-card">
          <div className="divide-y divide-border">
            {SPECS.map((spec) => (
              <div
                key={spec.label}
                className="flex items-center justify-between px-4 py-3 text-sm"
              >
                <dt className="text-muted-foreground">{spec.label}</dt>
                <dd className="font-mono tabular-nums">{spec.value}</dd>
              </div>
            ))}
          </div>
        </dl>
      </section>

      <section className="px-4 pb-10">
        <div className="flex items-baseline justify-between">
          <p className="font-display text-2xl leading-none tracking-tight">
            $199
          </p>
          <p className="text-[11px] uppercase tracking-[0.18em] text-muted-foreground">
            USD
          </p>
        </div>
        <Button
          size="lg"
          disabled
          className="mt-4 h-12 w-full text-base"
          aria-label="Buy Landlink Module I, coming soon"
        >
          Coming soon
        </Button>
        <p className="mt-3 text-xs leading-relaxed text-muted-foreground">
          Pre-orders open later this year. Join the waitlist from the support
          drawer to be notified.
        </p>
      </section>
    </main>
  );
}

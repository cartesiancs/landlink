import { useEffect, useState } from "react";

import { ROUTES } from "@/shared/config";
import { cn } from "@/shared/lib";
import { BackButton, Button } from "@/shared/ui";

type Spec = {
  label: string;
  value: string;
};

const SPECS: readonly Spec[] = [
  { label: "Range", value: "Up to 50 km" },
  { label: "Flight time", value: "32 min" },
  { label: "Camera", value: "4K · 60 fps" },
  { label: "Comms", value: "LoRa mesh" },
  { label: "Weight", value: "1.2 kg" },
  { label: "Battery", value: "5200 mAh swap" },
];

export function LandlinkOnePage() {
  const [isFooterVisible, setIsFooterVisible] = useState(true);

  useEffect(() => {
    let lastY = window.scrollY;
    const handleScroll = () => {
      const currentY = window.scrollY;
      const delta = currentY - lastY;
      if (Math.abs(delta) < 4) return;
      if (delta > 0 && currentY > 32) {
        setIsFooterVisible(false);
      } else if (delta < 0) {
        setIsFooterVisible(true);
      }
      lastY = currentY;
    };
    window.addEventListener("scroll", handleScroll, { passive: true });
    return () => {
      window.removeEventListener("scroll", handleScroll);
    };
  }, []);

  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <header className="sticky top-0 z-10 flex items-center gap-2 bg-background/90 px-4 ps-1 pt-[max(env(safe-area-inset-top),0.75rem)] pb-3 backdrop-blur supports-backdrop-filter:bg-background/70">
        <BackButton fallback={ROUTES.home} aria-label="Back to Home" />
        <h1 className="text-base font-medium">Landlink I</h1>
      </header>

      <section className="px-4 pt-2 pb-6">
        <div className="flex aspect-[4/3] w-full items-center justify-center overflow-hidden rounded-2xl border border-border bg-muted">
          <img
            src="/images/drone.webp"
            alt="Landlink I"
            className="h-[78%] w-auto object-contain"
          />
        </div>
        <div className="mt-5">
          <h2 className="mt-1 font-display text-3xl leading-tight tracking-tight">
            Landlink I
          </h2>
          <p className="mt-3 text-sm leading-relaxed text-muted-foreground">
            A long-range autonomous drone built around an open LoRa mesh.
            Powered by cartesiancs' technology, it sets up with a single
            connection and stays linked across kilometers, free from cell
            coverage and centralized control.
          </p>
        </div>
      </section>

      <section className="px-4 pb-[calc(env(safe-area-inset-bottom,12px)+220px)]">
        <dl className="overflow-hidden rounded-2xl border border-border bg-card">
          <div className="divide-y divide-border">
            {SPECS.map((spec) => (
              <div
                key={spec.label}
                className="flex items-center justify-between px-4 py-3 text-sm"
              >
                <dt className="text-muted-foreground">{spec.label}</dt>
                <dd className="tabular-nums">{spec.value}</dd>
              </div>
            ))}
          </div>
        </dl>
      </section>

      <section
        aria-hidden={!isFooterVisible}
        className={cn(
          "fixed inset-x-0 bottom-0 z-20 mx-auto w-full max-w-[430px] bg-background/90 px-4 pt-4 pb-[calc(max(env(safe-area-inset-bottom),0.75rem)+0.75rem)] backdrop-blur transition-transform duration-300 ease-out supports-backdrop-filter:bg-background/70",
          isFooterVisible ? "translate-y-0" : "translate-y-full",
        )}
      >
        <div className="flex items-baseline justify-between">
          <p className="font-display text-2xl leading-none tracking-tight">
            $1,499
          </p>
          <p className="text-[11px] uppercase tracking-[0.18em] text-muted-foreground">
            USD
          </p>
        </div>
        <Button
          size="lg"
          disabled
          className="mt-4 h-12 w-full text-base"
          aria-label="Buy Landlink I, coming soon"
        >
          Coming soon
        </Button>
      </section>
    </main>
  );
}

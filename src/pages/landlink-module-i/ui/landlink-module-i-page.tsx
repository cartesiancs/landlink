import { useEffect, useState } from "react";

import { ROUTES } from "@/shared/config";
import { cn } from "@/shared/lib";
import { Button, toast } from "@/shared/ui";
import { PageHeader } from "@/widgets/page-header";

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
      <PageHeader
        title="Landlink Module I"
        fallback={ROUTES.home}
        backLabel="Back to Home"
      />

      <section className="px-4 pt-2 pb-6">
        <div className="flex aspect-[4/3] w-full items-center justify-center overflow-hidden rounded-2xl border border-border bg-muted">
          <img
            src="/images/moduleone.webp"
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
            $99
          </p>
          <p className="text-[11px] text-muted-foreground">One time</p>
        </div>
        <Button
          size="lg"
          className="mt-4 h-12 w-full text-base"
          aria-label="Pre order Landlink Module I"
          onClick={() => {
            toast.info("Pre orders are not yet supported.", {
              description: "We will open them soon. Thanks for your patience.",
            });
          }}
        >
          Pre order
        </Button>
      </section>
    </main>
  );
}

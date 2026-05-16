import { ROUTES } from "@/shared/config";
import { Button, toast } from "@/shared/ui";
import { PageHeader } from "@/widgets/page-header";

import { SPEC_GROUPS } from "../model/specs";
import { useConfigurator } from "../model/use-configurator";
import { DesktopPriceBanner } from "./desktop-price-banner";
import { MobilePriceFooter } from "./mobile-price-footer";
import { ProductImageCarousel } from "./product-image-carousel";
import { SpecSection } from "./spec-section";

function formatMoney(value: number): string {
  return `$${value.toLocaleString("en-US", { maximumFractionDigits: 0 })}`;
}

function formatMonthly(value: number): string {
  return `$${value.toLocaleString("en-US", {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2,
  })}`;
}

export function LandlinkModuleIBuyPage() {
  const { selections, total, monthly, select } = useConfigurator();

  const handleAddToBag = () => {
    toast.info("Cart isn't wired up yet.", {
      description: "We'll open ordering soon. Thanks for your patience.",
    });
  };

  return (
    <main className="min-h-dvh bg-background">
      <div className="lg:hidden">
        <PageHeader
          title="Buy Landlink Module I"
          fallback={ROUTES.landlinkModuleI}
          backLabel="Back to Landlink Module I"
        />
      </div>

      <DesktopPriceBanner total={total} />

      <div className="mx-auto w-full max-w-7xl">
        <div className="grid gap-8 px-4 pt-2 pb-[calc(env(safe-area-inset-bottom,12px)+24px)] lg:grid-cols-[minmax(0,1fr)_440px] lg:gap-12 lg:px-8 lg:pt-10 lg:pb-20">
          <div className="lg:sticky lg:top-20 lg:self-start">
            <ProductImageCarousel />
          </div>

          <section className="flex flex-col">
            <header className="lg:hidden">
              <p className="text-xs font-medium tracking-wide text-muted-foreground uppercase">
                Landlink
              </p>
              <h1 className="mt-1 font-display text-3xl leading-tight tracking-tight">
                Module I
              </h1>
              <p className="mt-3 text-sm leading-relaxed text-muted-foreground">
                Build your kit. Pick the band, the bundle, and the extras. We'll
                handle the rest.
              </p>
            </header>

            <header className="hidden lg:block lg:pb-2">
              <h1 className="mt-2 font-display text-4xl leading-tight tracking-tight">
                Landlink Module I.
                <br />
                <span className="text-muted-foreground">Built your way.</span>
              </h1>
              <p className="mt-4 max-w-md text-sm leading-relaxed text-muted-foreground">
                Pick the band that matches your region, choose how many nodes
                you want to place, and add the antennas and care that fit your
                setup.
              </p>
            </header>

            <div className="mt-6 space-y-8 lg:mt-10">
              {SPEC_GROUPS.map((group) => (
                <SpecSection
                  key={group.id}
                  group={group}
                  selectedId={selections[group.id]}
                  onSelect={(optionId) => {
                    select(group.id, optionId);
                  }}
                />
              ))}
            </div>

            <div className="mt-10 hidden rounded-2xl border border-border bg-card p-5 lg:block">
              <div className="flex items-baseline justify-between">
                <p className="text-sm font-medium text-muted-foreground">
                  Total
                </p>
                <p className="font-display text-3xl leading-none tracking-tight tabular-nums">
                  {formatMoney(total)}
                </p>
              </div>
              <Button
                size="lg"
                className="mt-5 h-12 w-full text-base"
                onClick={handleAddToBag}
              >
                Add to Bag
              </Button>
              <p className="mt-3 text-center text-[11px] text-muted-foreground">
                Free shipping. Pick up available at most Stores.
              </p>
            </div>

            <MobilePriceFooter
              total={total}
              monthly={monthly}
              onAddToBag={handleAddToBag}
            />
          </section>
        </div>
      </div>
    </main>
  );
}

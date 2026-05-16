import { ROUTES } from "@/shared/config";
import { BackButton } from "@/shared/ui";

type DesktopPriceBannerProps = {
  total: number;
};

function formatMoney(value: number): string {
  return `$${value.toLocaleString("en-US", { maximumFractionDigits: 0 })}`;
}

export function DesktopPriceBanner({ total }: DesktopPriceBannerProps) {
  return (
    <div className="sticky top-0 z-30 hidden border-b border-border bg-background/85 backdrop-blur supports-backdrop-filter:bg-background/70 lg:block">
      <div className="mx-auto flex h-14 max-w-7xl items-center justify-between gap-3 px-4">
        <div className="flex min-w-0 items-center gap-2">
          <BackButton
            fallback={ROUTES.landlinkModuleI}
            aria-label="Back to Landlink Module I"
            className="-ms-1"
          />
          <p className="truncate text-base font-medium tracking-tight">
            Landlink Module I
          </p>
        </div>
        <div className="flex items-center gap-8 text-sm">
          <p className="text-foreground tabular-nums">
            From {formatMoney(total)}
          </p>
        </div>
      </div>
    </div>
  );
}

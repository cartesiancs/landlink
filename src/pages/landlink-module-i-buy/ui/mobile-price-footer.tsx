import { Button } from "@/shared/ui";

type MobilePriceFooterProps = {
  total: number;
  onAddToBag: () => void;
};

function formatMoney(value: number): string {
  return `$${value.toLocaleString("en-US", { maximumFractionDigits: 0 })}`;
}

export function MobilePriceFooter({
  total,
  onAddToBag,
}: MobilePriceFooterProps) {
  return (
    <div className="mt-10 rounded-2xl border border-border bg-card p-5 lg:hidden">
      <div className="flex items-baseline justify-between">
        <p className="text-sm font-medium text-muted-foreground">Total</p>
        <p className="font-display text-3xl leading-none tracking-tight tabular-nums">
          {formatMoney(total)}
        </p>
      </div>
      <Button
        size="lg"
        className="mt-5 h-12 w-full text-base"
        onClick={onAddToBag}
      >
        Add to Bag
      </Button>
      <p className="mt-3 text-center text-[11px] text-muted-foreground">
        Free shipping. Pick up available at most Stores.
      </p>
    </div>
  );
}

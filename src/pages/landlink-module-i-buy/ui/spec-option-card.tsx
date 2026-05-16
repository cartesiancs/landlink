import { cn } from "@/shared/lib";

import type { SpecOption } from "../model/specs";

type SpecOptionCardProps = {
  option: SpecOption;
  selected: boolean;
  onSelect: () => void;
};

function formatPriceDelta(priceDelta: number): string {
  if (priceDelta === 0) return "Included";
  return `+$${priceDelta.toLocaleString("en-US")}`;
}

export function SpecOptionCard({
  option,
  selected,
  onSelect,
}: SpecOptionCardProps) {
  return (
    <button
      type="button"
      role="radio"
      aria-checked={selected}
      onClick={onSelect}
      className={cn(
        "group flex w-full items-start gap-3 rounded-2xl border bg-card p-4 text-left transition-all outline-none",
        "focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/40",
        selected
          ? "border-foreground ring-2 ring-foreground/15"
          : "border-border hover:border-foreground/40",
      )}
    >
      <span
        aria-hidden
        className={cn(
          "mt-0.5 flex size-5 shrink-0 items-center justify-center rounded-full border transition-colors",
          selected
            ? "border-foreground bg-foreground"
            : "border-border bg-background",
        )}
      >
        <span
          className={cn(
            "size-1.5 rounded-full bg-background transition-opacity",
            selected ? "opacity-100" : "opacity-0",
          )}
        />
      </span>

      <span className="flex min-w-0 flex-1 flex-col gap-1">
        <span className="flex items-baseline justify-between gap-3">
          <span className="text-sm font-semibold leading-tight">
            {option.label}
          </span>
          {option.badge ? (
            <span className="shrink-0 rounded-full bg-foreground px-2 py-0.5 text-[10px] font-medium tracking-wide text-background uppercase">
              {option.badge}
            </span>
          ) : null}
        </span>
        {option.description ? (
          <span className="text-xs leading-relaxed text-muted-foreground">
            {option.description}
          </span>
        ) : null}
        <span
          className={cn(
            "mt-1 text-xs tabular-nums",
            option.priceDelta === 0
              ? "text-muted-foreground"
              : "font-medium text-foreground",
          )}
        >
          {formatPriceDelta(option.priceDelta)}
        </span>
      </span>
    </button>
  );
}

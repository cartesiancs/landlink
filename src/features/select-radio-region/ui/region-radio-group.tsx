import { hapticTick } from "@/shared/lib";
import { REGION_OPTIONS, isRegionValue } from "@/shared/config";
import { RadioGroup, RadioGroupItem } from "@/shared/ui";

import { useRadioRegion } from "../model/use-radio-region";

export function RegionRadioGroup() {
  const { region, isConnected, isPending, select } = useRadioRegion();

  const disabled = !isConnected || isPending;

  return (
    <RadioGroup
      value={region !== null ? String(region) : null}
      disabled={disabled}
      onValueChange={(value) => {
        const parsed = Number.parseInt(value, 10);
        if (!Number.isFinite(parsed) || !isRegionValue(parsed)) return;
        hapticTick();
        void select(parsed);
      }}
      aria-label="Select radio region"
      className="gap-2"
    >
      {REGION_OPTIONS.map((opt) => {
        const id = `region-${opt.code}`;
        const value = String(opt.value);
        return (
          <label
            key={opt.code}
            htmlFor={id}
            className="flex cursor-pointer items-start gap-3 rounded-md border border-border px-4 py-3 transition-colors hover:bg-muted data-[disabled=true]:cursor-not-allowed data-[disabled=true]:opacity-60"
            data-disabled={disabled}
          >
            <RadioGroupItem id={id} value={value} className="mt-0.5" />
            <div className="flex min-w-0 flex-col gap-0.5">
              <span className="text-sm font-medium leading-none">
                {opt.label}
              </span>
              <span className="text-xs text-muted-foreground">
                {opt.freqRange} · {opt.dutyCycle} · {opt.txPower}
              </span>
            </div>
          </label>
        );
      })}
    </RadioGroup>
  );
}

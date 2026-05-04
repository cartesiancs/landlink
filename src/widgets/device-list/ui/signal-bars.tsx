import { cn } from "@/shared/lib";
import { signalBars } from "@/entities/registered-device";

type SignalBarsProps = {
  signalDbm: number | null;
  className?: string;
};

const BARS = [1, 2, 3, 4] as const;

export function SignalBars({ signalDbm, className }: SignalBarsProps) {
  const active = signalBars(signalDbm);
  return (
    <div
      className={cn("flex items-end gap-[2px]", className)}
      aria-label={
        signalDbm === null ? "Signal unknown" : `Signal ${active.toString()} of 4`
      }
      role="img"
    >
      {BARS.map((bar) => (
        <span
          key={bar}
          className={cn(
            "w-[3px] rounded-sm transition-colors",
            bar === 1 && "h-1",
            bar === 2 && "h-2",
            bar === 3 && "h-3",
            bar === 4 && "h-4",
            bar <= active ? "bg-foreground" : "bg-muted",
          )}
        />
      ))}
    </div>
  );
}

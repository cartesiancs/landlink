import { Moon, Sun } from "lucide-react";

import { useTheme } from "@/entities/theme";
import { cn, hapticTick } from "@/shared/lib";

export function ThemeToggle() {
  const { theme, toggleTheme } = useTheme();
  const isDark = theme === "dark";

  return (
    <button
      type="button"
      role="switch"
      aria-checked={isDark}
      aria-label={isDark ? "Switch to light mode" : "Switch to dark mode"}
      onClick={() => {
        hapticTick();
        toggleTheme();
      }}
      className="relative inline-flex h-7 w-12 shrink-0 items-center rounded-full border border-border bg-muted transition-colors"
    >
      <span
        className={cn(
          "absolute top-1/2 flex size-5 -translate-y-1/2 items-center justify-center rounded-full bg-background text-foreground shadow-sm transition-transform duration-200 ease-out",
          isDark ? "translate-x-5.5" : "translate-x-0.5",
        )}
      >
        {isDark ? (
          <Moon className="size-3" aria-hidden="true" />
        ) : (
          <Sun className="size-3" aria-hidden="true" />
        )}
      </span>
    </button>
  );
}

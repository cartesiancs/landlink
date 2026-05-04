import { setDebugMode, useDebugMode } from "@/entities/debug-mode";
import { hapticTick } from "@/shared/lib";
import { Switch } from "@/shared/ui";

export function DebugModeToggle() {
  const enabled = useDebugMode();
  return (
    <label className="flex w-full items-center justify-between gap-4 rounded-md px-1 py-2">
      <span className="flex flex-col">
        <span className="text-sm font-medium">Debug mode</span>
      </span>
      <Switch
        checked={enabled}
        onCheckedChange={(next) => {
          hapticTick();
          setDebugMode(next);
        }}
        aria-label="Debug mode"
      />
    </label>
  );
}

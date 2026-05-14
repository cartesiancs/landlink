import { HelpCircle, Menu } from "lucide-react";

import { hapticTick } from "@/shared/lib";
import { Button } from "@/shared/ui";

type AppHeaderProps = {
  onMenuOpen: () => void;
  onSupportOpen: () => void;
};

export function AppHeader({ onMenuOpen, onSupportOpen }: AppHeaderProps) {
  return (
    <header data-vt-name="app-header" className="shrink-0">
      <div aria-hidden className="h-[env(safe-area-inset-top)] bg-background" />
      <div className="flex h-14 items-center justify-between bg-background px-2">
        <Button
          variant="ghost"
          size="icon"
          aria-label="Open menu"
          onClick={() => {
            hapticTick();
            onMenuOpen();
          }}
        >
          <Menu />
        </Button>
        <Button
          variant="ghost"
          size="icon"
          aria-label="Open support"
          onClick={() => {
            hapticTick();
            onSupportOpen();
          }}
        >
          <HelpCircle />
        </Button>
      </div>
    </header>
  );
}

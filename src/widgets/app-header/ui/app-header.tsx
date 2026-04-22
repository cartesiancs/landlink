import { HelpCircle, Menu } from "lucide-react";

import { Button } from "@/shared/ui";

type AppHeaderProps = {
  onMenuOpen: () => void;
  onSupportOpen: () => void;
};

export function AppHeader({ onMenuOpen, onSupportOpen }: AppHeaderProps) {
  return (
    <header className="sticky top-0 z-30 flex h-14 items-center justify-between bg-background/80 px-3 backdrop-blur supports-backdrop-filter:bg-background/60">
      <Button
        variant="ghost"
        size="icon"
        aria-label="Open menu"
        onClick={onMenuOpen}
      >
        <Menu />
      </Button>
      <Button
        variant="ghost"
        size="icon"
        aria-label="Open support"
        onClick={onSupportOpen}
      >
        <HelpCircle />
      </Button>
    </header>
  );
}

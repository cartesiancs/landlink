import { HelpCircle, Menu } from "lucide-react";

import { hapticTick } from "@/shared/lib";
import { BackButton, Button } from "@/shared/ui";

type AppHeaderProps = {
  // Omit to hide the menu button entirely (e.g. the map page on web).
  onMenuOpen?: () => void;
  onSupportOpen: () => void;
  // Renders a back affordance before the menu button. Used on the map page
  // on web, where the fullscreen map has no other way to navigate back.
  showBack?: boolean;
};

export function AppHeader({
  onMenuOpen,
  onSupportOpen,
  showBack = false,
}: AppHeaderProps) {
  return (
    <header data-vt-name="app-header" className="shrink-0">
      <div aria-hidden className="h-[env(safe-area-inset-top)] bg-background" />
      <div className="flex h-14 items-center justify-between bg-background px-2">
        <div className="flex items-center">
          {showBack && <BackButton />}
          {onMenuOpen && (
            <Button
              variant="ghost"
              size="icon"
              aria-label="Open menu"
              onClick={() => {
                hapticTick();
                onMenuOpen?.();
              }}
            >
              <Menu />
            </Button>
          )}
        </div>
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

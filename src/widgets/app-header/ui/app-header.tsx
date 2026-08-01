import { HelpCircle, Menu } from "lucide-react";
import type { ReactNode } from "react";

import { cn, hapticTick } from "@/shared/lib";
import { BackButton, Button } from "@/shared/ui";

type AppHeaderProps = {
  // Omit to hide the menu button entirely (e.g. the map page on web).
  onMenuOpen?: () => void;
  onSupportOpen: () => void;
  // Renders a back affordance before the menu button. Used on the map page
  // on web, where the fullscreen map has no other way to navigate back.
  showBack?: boolean;
  // Extra buttons rendered before the support button (e.g. map recenter).
  actions?: ReactNode;
  // Rounds the bottom edge of the bar. Used on the map page, where the
  // header sits on top of the fullscreen map instead of page content.
  roundedBottom?: boolean;
};

export function AppHeader({
  onMenuOpen,
  onSupportOpen,
  showBack = false,
  actions,
  roundedBottom = false,
}: AppHeaderProps) {
  return (
    <header data-vt-name="app-header" className="shrink-0">
      <div aria-hidden className="h-[env(safe-area-inset-top)] bg-background" />
      <div
        className={cn(
          "flex h-14 items-center justify-between bg-background px-2",
          roundedBottom && "rounded-b-2xl",
        )}
      >
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
        <div className="flex items-center">
          {actions}
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
      </div>
    </header>
  );
}

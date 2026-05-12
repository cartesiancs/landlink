import { Home, List, Settings } from "lucide-react";
import type { ComponentType, SVGProps } from "react";
import { Link, useLocation } from "react-router-dom";

import { ROUTES, type RoutePath } from "@/shared/config";
import { cn, hapticTick } from "@/shared/lib";

import { useBottomNavVisible } from "../model/use-bottom-nav-visible";

type NavItem = {
  to: RoutePath;
  label: string;
  Icon: ComponentType<SVGProps<SVGSVGElement>>;
  state?: { fromNav: true };
};

const ITEMS: readonly NavItem[] = [
  { to: ROUTES.home, label: "Home", Icon: Home, state: { fromNav: true } },
  { to: ROUTES.lists, label: "Lists", Icon: List },
  { to: ROUTES.settings, label: "Settings", Icon: Settings },
];

export function BottomNavBar() {
  const visible = useBottomNavVisible();
  const { pathname } = useLocation();
  if (!visible) return null;

  return (
    <div className="pointer-events-none fixed inset-x-0 bottom-0 z-40 flex justify-center [view-transition-name:bottom-nav-bar]">
      <div className="pointer-events-auto w-full max-w-[430px]">
        <nav
          aria-label="Bottom navigation"
          className="flex border-t border-border bg-background/90 backdrop-blur pt-2 supports-backdrop-filter:bg-background/70"
        >
          {ITEMS.map(({ to, label, Icon, state }) => {
            const active = pathname === to;
            return (
              <Link
                key={to}
                to={to}
                state={state}
                viewTransition
                aria-label={label}
                aria-current={active ? "page" : undefined}
                onClick={() => {
                  hapticTick();
                }}
                className={cn(
                  "flex flex-1 flex-col items-center justify-center gap-1 py-2.5 text-[11px] font-medium transition-colors",
                  active ? "text-foreground" : "text-muted-foreground",
                )}
              >
                <Icon className="h-5 w-5" aria-hidden />
              </Link>
            );
          })}
        </nav>
        {/* WHY: solid bg strip under the nav prevents iOS Safari URL bar tint
            from sampling the translucent layer above. */}
        <div
          aria-hidden
          className="h-[max(env(safe-area-inset-bottom),0px)] bg-background"
        />
      </div>
    </div>
  );
}

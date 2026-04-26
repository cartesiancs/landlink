import { type MouseEvent } from "react";
import {
  Building2,
  FileText,
  House,
  Info,
  Package,
  Plane,
  ShieldCheck,
  type LucideIcon,
} from "lucide-react";
import { Link, useLocation } from "react-router-dom";
import { Capacitor } from "@capacitor/core";

import {
  Sheet,
  SheetContent,
  SheetDescription,
  SheetHeader,
  SheetTitle,
} from "@/shared/ui";
import { cn } from "@/shared/lib";
import { ROUTES } from "@/shared/config";
import { ThemeToggle } from "@/features/toggle-theme";

const IS_NATIVE_APP = Capacitor.isNativePlatform();

type NavigationSidebarProps = {
  open: boolean;
  onOpenChange: (open: boolean) => void;
};

type NavItem = {
  label: string;
  to: string;
  icon: LucideIcon;
};

const NAV_ITEMS: readonly NavItem[] = [
  { label: "Home", to: ROUTES.home, icon: House },
  { label: "Landlink I", to: ROUTES.landlinkOne, icon: Plane },
  { label: "Landlink Module I", to: ROUTES.landlinkModuleI, icon: Package },
  { label: "About", to: ROUTES.about, icon: Info },
];

const LEGAL_ITEMS: readonly NavItem[] = [
  { label: "Privacy", to: ROUTES.privacy, icon: ShieldCheck },
  { label: "Terms", to: ROUTES.terms, icon: FileText },
];

export function NavigationSidebar({
  open,
  onOpenChange,
}: NavigationSidebarProps) {
  const location = useLocation();
  const handleClose = () => {
    onOpenChange(false);
  };

  const handleNavClick = (to: string) => (event: MouseEvent) => {
    if (location.pathname === to) {
      event.preventDefault();
    }
    if (to === ROUTES.home) {
      handleClose();
    }
  };

  return (
    <Sheet open={open} onOpenChange={onOpenChange}>
      <SheetContent side="left" className="w-72">
        <SheetHeader
          className={cn(
            IS_NATIVE_APP
              ? "pt-[calc(max(env(safe-area-inset-top),1rem)+18px)]"
              : "pt-[max(env(safe-area-inset-top),1rem)]",
          )}
        >
          <SheetTitle>Landlink</SheetTitle>
          <SheetDescription></SheetDescription>
        </SheetHeader>
        <nav className="mt-2 flex flex-col gap-1 px-4">
          {NAV_ITEMS.map((item) => {
            const Icon = item.icon;
            return (
              <Link
                key={item.to}
                to={item.to}
                viewTransition
                onClick={handleNavClick(item.to)}
                className="flex items-center gap-2 rounded-md px-1 py-2 text-sm transition-[padding] duration-200 hover:bg-muted hover:px-3"
              >
                <Icon className="size-4" aria-hidden="true" />
                {item.label}
              </Link>
            );
          })}
        </nav>
        <div className="mt-auto flex flex-col gap-1 px-4 pt-4 pb-[max(env(safe-area-inset-bottom),1rem)]">
          <div className="flex items-center justify-between gap-3 px-1 pb-3">
            <span className="text-xs font-medium text-muted-foreground">
              Theme
            </span>
            <ThemeToggle />
          </div>
          <nav className="flex flex-col gap-1 border-t border-border pt-3">
            {LEGAL_ITEMS.map((item) => {
              const Icon = item.icon;
              return (
                <Link
                  key={item.to}
                  to={item.to}
                  onClick={handleNavClick(item.to)}
                  className="flex items-center gap-2 rounded-md px-1 py-2 text-sm transition-[padding] duration-200 hover:bg-muted hover:px-3"
                >
                  <Icon className="size-4" aria-hidden="true" />
                  {item.label}
                </Link>
              );
            })}
          </nav>
          <a
            href="https://cartesiancs.com"
            target="_blank"
            rel="noreferrer noopener"
            className="flex items-center gap-2 rounded-md px-1 py-2 text-sm transition-[padding] duration-200 hover:bg-muted hover:px-3"
          >
            <Building2 className="size-4" aria-hidden="true" />
            cartesiancs
          </a>
          <p className="mt-1 px-1 text-xs leading-relaxed text-muted-foreground">
            © {new Date().getFullYear()} cartesiancs.
            <br />
            All rights reserved.
          </p>
        </div>
      </SheetContent>
    </Sheet>
  );
}

import { type MouseEvent } from "react";
import {
  Building2,
  Cpu,
  FileText,
  Hash as HashIcon,
  House,
  Info,
  List,
  Package,
  Plane,
  Settings,
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
import { useRegisteredDevices } from "@/entities/registered-device";

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
  { label: "Lists", to: ROUTES.lists, icon: List },
  { label: "Channels", to: ROUTES.channels, icon: HashIcon },
  { label: "Settings", to: ROUTES.settings, icon: Settings },
  { label: "About", to: ROUTES.about, icon: Info },
];

const PRODUCT_ITEMS: readonly NavItem[] = [
  { label: "Landlink I", to: ROUTES.landlinkOne, icon: Plane },
  { label: "Landlink Module I", to: ROUTES.landlinkModuleI, icon: Package },
  { label: "Landlink Firmware", to: ROUTES.landlinkFirmware, icon: Cpu },
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
  const devices = useRegisteredDevices();
  const hasAnyDevice = devices.length > 0;
  const visibleNavItems = NAV_ITEMS.filter((item) => {
    if (item.to === ROUTES.lists) return hasAnyDevice;
    if (item.to === ROUTES.channels) return hasAnyDevice;
    return true;
  });
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
          {visibleNavItems.map((item) => {
            const Icon = item.icon;
            return (
              <Link
                key={item.to}
                to={item.to}
                state={item.to === ROUTES.home ? { fromNav: true } : undefined}
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
        <nav className="mt-4 flex flex-col gap-1 px-4">
          <span className="px-1 pb-1 text-xs font-medium text-muted-foreground">
            Product
          </span>
          {PRODUCT_ITEMS.map((item) => {
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

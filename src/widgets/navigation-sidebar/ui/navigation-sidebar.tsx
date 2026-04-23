import {
  Building2,
  FileText,
  House,
  Info,
  ShieldCheck,
  type LucideIcon,
} from "lucide-react";
import { Link } from "react-router-dom";

import {
  Sheet,
  SheetContent,
  SheetDescription,
  SheetHeader,
  SheetTitle,
} from "@/shared/ui";
import { ROUTES } from "@/shared/config";

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
  const handleClose = () => {
    onOpenChange(false);
  };

  return (
    <Sheet open={open} onOpenChange={onOpenChange}>
      <SheetContent side="left" className="w-72">
        <SheetHeader>
          <SheetTitle>Menu</SheetTitle>
          <SheetDescription></SheetDescription>
        </SheetHeader>
        <nav className="mt-4 flex flex-col gap-1 px-4">
          {NAV_ITEMS.map((item) => {
            const Icon = item.icon;
            return (
              <Link
                key={item.to}
                to={item.to}
                onClick={handleClose}
                className="flex items-center gap-2 rounded-md px-1 py-2 text-sm hover:bg-muted"
              >
                <Icon className="size-4" aria-hidden="true" />
                {item.label}
              </Link>
            );
          })}
        </nav>
        <div className="mt-auto flex flex-col gap-1 p-4">
          <nav className="flex flex-col gap-1 border-t border-border pt-3">
            {LEGAL_ITEMS.map((item) => {
              const Icon = item.icon;
              return (
                <Link
                  key={item.to}
                  to={item.to}
                  onClick={handleClose}
                  className="flex items-center gap-2 rounded-md px-1 py-2 text-sm hover:bg-muted"
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
            className="flex items-center gap-2 rounded-md px-1 py-2 text-sm hover:bg-muted"
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

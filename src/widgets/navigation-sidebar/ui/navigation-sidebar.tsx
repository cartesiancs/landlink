import { Building2, House, Info, type LucideIcon } from "lucide-react";
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

export function NavigationSidebar({
  open,
  onOpenChange,
}: NavigationSidebarProps) {
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
                onClick={() => {
                  onOpenChange(false);
                }}
                className="flex items-center gap-2 rounded-md px-1 py-2 text-sm hover:bg-muted"
              >
                <Icon className="size-4" aria-hidden="true" />
                {item.label}
              </Link>
            );
          })}
        </nav>
        <div className="mt-auto flex flex-col gap-2 p-4">
          <a
            href="https://cartesiancs.com"
            target="_blank"
            rel="noreferrer noopener"
            className="flex items-center gap-2 rounded-md px-1 py-2 text-sm hover:bg-muted"
          >
            <Building2 className="size-4" aria-hidden="true" />
            cartesiancs
          </a>
          <p className="px-1 text-xs leading-relaxed text-muted-foreground">
            © {new Date().getFullYear()} cartesiancs.
            <br />
            All rights reserved.
          </p>
        </div>
      </SheetContent>
    </Sheet>
  );
}

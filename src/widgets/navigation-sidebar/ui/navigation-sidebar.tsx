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
};

const NAV_ITEMS: readonly NavItem[] = [
  { label: "Home", to: ROUTES.home },
  { label: "About", to: ROUTES.about },
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
          <SheetDescription>Navigate the app.</SheetDescription>
        </SheetHeader>
        <nav className="mt-4 flex flex-col gap-1 px-4">
          {NAV_ITEMS.map((item) => (
            <Link
              key={item.to}
              to={item.to}
              onClick={() => {
                onOpenChange(false);
              }}
              className="rounded-md px-3 py-2 text-sm hover:bg-muted"
            >
              {item.label}
            </Link>
          ))}
        </nav>
      </SheetContent>
    </Sheet>
  );
}

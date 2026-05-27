import { Link } from "react-router-dom";

import {
  Drawer,
  DrawerContent,
  DrawerDescription,
  DrawerHeader,
  DrawerTitle,
} from "@/shared/ui";
import { ROUTES } from "@/shared/config";

type SupportDrawerProps = {
  open: boolean;
  onOpenChange: (open: boolean) => void;
};

export function SupportDrawer({ open, onOpenChange }: SupportDrawerProps) {
  return (
    <Drawer open={open} onOpenChange={onOpenChange}>
      <DrawerContent className="sm:mx-auto sm:max-w-md">
        <DrawerHeader>
          <DrawerTitle>Support</DrawerTitle>
          <DrawerDescription>
            Need help? Browse FAQs or contact us.
          </DrawerDescription>
        </DrawerHeader>
        <div className="flex flex-col gap-3 px-4 pb-[max(env(safe-area-inset-bottom),1.5rem)] text-sm">
          <a
            href="mailto:jun@cartesiancs.com"
            className="rounded-md border border-border px-3 py-3 hover:bg-muted"
          >
            Email support
          </a>
          <Link
            to={ROUTES.faq}
            viewTransition
            onClick={() => {
              onOpenChange(false);
            }}
            className="rounded-md border border-border px-3 py-3 hover:bg-muted"
          >
            Read the FAQ
          </Link>
        </div>
      </DrawerContent>
    </Drawer>
  );
}

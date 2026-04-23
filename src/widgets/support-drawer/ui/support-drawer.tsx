import {
  Drawer,
  DrawerContent,
  DrawerDescription,
  DrawerHeader,
  DrawerTitle,
} from "@/shared/ui";

type SupportDrawerProps = {
  open: boolean;
  onOpenChange: (open: boolean) => void;
};

export function SupportDrawer({ open, onOpenChange }: SupportDrawerProps) {
  return (
    <Drawer open={open} onOpenChange={onOpenChange}>
      <DrawerContent>
        <DrawerHeader>
          <DrawerTitle>Support</DrawerTitle>
          <DrawerDescription>
            Need help? Browse FAQs or contact us.
          </DrawerDescription>
        </DrawerHeader>
        <div className="flex flex-col gap-3 px-4 pb-6 text-sm">
          <a
            href="mailto:jun@cartesiancs.com"
            className="rounded-md border border-border px-3 py-3 hover:bg-muted"
          >
            Email support
          </a>
          <a
            href="#"
            className="rounded-md border border-border px-3 py-3 hover:bg-muted"
          >
            Read the FAQ
          </a>
        </div>
      </DrawerContent>
    </Drawer>
  );
}

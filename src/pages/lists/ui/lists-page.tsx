import { useState } from "react";
import { Plus } from "lucide-react";
import { useNavigate } from "react-router-dom";

import { ROUTES } from "@/shared/config";
import { hapticTick } from "@/shared/lib";
import { Button } from "@/shared/ui";
import { AppHeader } from "@/widgets/app-header";
import { DeviceList } from "@/widgets/device-list";
import { NavigationSidebar } from "@/widgets/navigation-sidebar";
import { SupportDrawer } from "@/widgets/support-drawer";

export function ListsPage() {
  const navigate = useNavigate();
  const [sidebarOpen, setSidebarOpen] = useState(false);
  const [supportOpen, setSupportOpen] = useState(false);

  return (
    <div className="mx-auto flex h-dvh w-full max-w-[430px] flex-col bg-background">
      <AppHeader
        onMenuOpen={() => {
          setSidebarOpen(true);
        }}
        onSupportOpen={() => {
          setSupportOpen(true);
        }}
      />
      <div className="flex items-baseline justify-between px-4 pt-1 pb-3">
        <h1 className="text-base font-medium">Devices</h1>
      </div>
      <main className="min-h-0 flex-1 overflow-y-auto px-4 pb-[max(env(safe-area-inset-bottom),1.5rem)]">
        <Button
          variant="outline"
          className="mb-4 w-full justify-start gap-2"
          onClick={() => {
            hapticTick();
            void navigate(ROUTES.connectBluetooth, { viewTransition: true });
          }}
        >
          <Plus className="size-4" aria-hidden="true" />
          Connect new device
        </Button>
        <DeviceList />
      </main>
      <NavigationSidebar open={sidebarOpen} onOpenChange={setSidebarOpen} />
      <SupportDrawer open={supportOpen} onOpenChange={setSupportOpen} />
    </div>
  );
}

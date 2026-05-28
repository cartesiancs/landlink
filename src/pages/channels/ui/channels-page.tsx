import { Plus } from "lucide-react";
import { useState } from "react";

import { useLandlinkDevice } from "@/entities/landlink-device";
import { findDevice, useRegisteredDevices } from "@/entities/registered-device";
import { CreateChannelDialog } from "@/features/create-channel";
import { cn, hapticTick } from "@/shared/lib";
import { Button } from "@/shared/ui";
import { AppHeader } from "@/widgets/app-header";
import { BottomNavBar, useBottomNavVisible } from "@/widgets/bottom-nav-bar";
import { ChannelList } from "@/widgets/channel-list";
import { NavigationSidebar } from "@/widgets/navigation-sidebar";
import { SupportDrawer } from "@/widgets/support-drawer";

export function ChannelsPage() {
  const [sidebarOpen, setSidebarOpen] = useState(false);
  const [supportOpen, setSupportOpen] = useState(false);
  const [createOpen, setCreateOpen] = useState(false);
  const navVisible = useBottomNavVisible();
  const device = useLandlinkDevice();
  const registeredDevices = useRegisteredDevices();
  const registered = device
    ? findDevice(registeredDevices, device.deviceId)
    : null;
  // Meshtastic channels are device-managed (added/removed via admin_message
  // writes to NVS). Local-only creation would lie to the user about what's
  // configured on-device, so we hide the affordance for that family.
  const canCreateLocally = registered?.protocol !== "meshtastic";

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
      <div className="px-4 pt-1 pb-3">
        <h1 className="text-base font-medium">Channels</h1>
      </div>
      <main
        className={cn(
          "min-h-0 flex-1 overflow-y-auto px-4 pt-6",
          navVisible
            ? "pb-[calc(max(env(safe-area-inset-bottom),0.75rem)+4.5rem)]"
            : "pb-[max(env(safe-area-inset-bottom),1.5rem)]",
        )}
      >
        <ChannelList />
        {canCreateLocally ? (
          <Button
            variant="outline"
            size="lg"
            className="mt-4 h-12 w-full justify-start gap-2"
            onClick={() => {
              hapticTick();
              setCreateOpen(true);
            }}
          >
            <Plus className="size-4" aria-hidden="true" />
            Create new channel
          </Button>
        ) : (
          <p className="mt-4 rounded-md border border-dashed border-border px-3 py-3 text-center text-xs text-muted-foreground">
            Channels are managed on the Meshtastic device. Use the official
            Meshtastic app to add or remove channels.
          </p>
        )}
      </main>
      <NavigationSidebar open={sidebarOpen} onOpenChange={setSidebarOpen} />
      <SupportDrawer open={supportOpen} onOpenChange={setSupportOpen} />
      <CreateChannelDialog open={createOpen} onOpenChange={setCreateOpen} />
      <BottomNavBar />
    </div>
  );
}

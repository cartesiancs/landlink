import { Plus } from "lucide-react";
import { useState } from "react";

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
      <main
        className={cn(
          "min-h-0 flex-1 overflow-y-auto px-4 pt-6",
          navVisible
            ? "pb-[calc(max(env(safe-area-inset-bottom),0.75rem)+4.5rem)]"
            : "pb-[max(env(safe-area-inset-bottom),1.5rem)]",
        )}
      >
        <h1 className="mb-4 text-lg font-semibold tracking-tight">Channels</h1>
        <ChannelList />
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
      </main>
      <NavigationSidebar open={sidebarOpen} onOpenChange={setSidebarOpen} />
      <SupportDrawer open={supportOpen} onOpenChange={setSupportOpen} />
      <CreateChannelDialog open={createOpen} onOpenChange={setCreateOpen} />
      <BottomNavBar />
    </div>
  );
}

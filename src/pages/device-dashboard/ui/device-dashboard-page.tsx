import { useState } from "react";
import { ChevronLeft } from "lucide-react";
import { useNavigate } from "react-router-dom";

import { useLandlinkDevice } from "@/entities/landlink-device";
import { SendMeshForm } from "@/features/send-mesh-message";
import { ROUTES } from "@/shared/config";
import { hapticTick } from "@/shared/lib";
import { Button } from "@/shared/ui";
import { AppHeader } from "@/widgets/app-header";
import { DeviceTelemetryCard } from "@/widgets/device-telemetry-card";
import { MeshMessageFeed } from "@/widgets/mesh-message-feed";
import { NavigationSidebar } from "@/widgets/navigation-sidebar";
import { SupportDrawer } from "@/widgets/support-drawer";

export function DeviceDashboardPage() {
  const navigate = useNavigate();
  const device = useLandlinkDevice();
  const [sidebarOpen, setSidebarOpen] = useState(false);
  const [supportOpen, setSupportOpen] = useState(false);

  const isConnected = device?.status === "connected";

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
      <main className="flex min-h-0 flex-1 flex-col gap-4 overflow-y-auto px-4 pt-4 pb-[max(env(safe-area-inset-bottom),1rem)]">
        <div className="flex items-center gap-2">
          <Button
            variant="ghost"
            size="icon"
            aria-label="Back"
            onClick={() => {
              hapticTick();
              void navigate(ROUTES.lists);
            }}
          >
            <ChevronLeft className="size-4" aria-hidden="true" />
          </Button>
          <div className="min-w-0 flex-1">
            <h1 className="truncate text-base font-semibold">
              {device?.name ?? "No device"}
            </h1>
            <p className="text-xs text-muted-foreground">
              {isConnected
                ? device.info?.nodeId
                  ? `node ${device.info.nodeId}`
                  : "Connected"
                : "Not connected"}
            </p>
          </div>
        </div>

        {isConnected ? (
          <>
            <DeviceTelemetryCard />
            <MeshMessageFeed />
            <SendMeshForm />
          </>
        ) : (
          <div className="flex flex-1 flex-col items-center justify-center gap-3 text-center">
            <p className="text-sm text-muted-foreground">
              No device connected.
            </p>
            <Button
              variant="outline"
              onClick={() => {
                hapticTick();
                void navigate(ROUTES.connectBluetooth);
              }}
            >
              Connect a device
            </Button>
          </div>
        )}
      </main>
      <NavigationSidebar open={sidebarOpen} onOpenChange={setSidebarOpen} />
      <SupportDrawer open={supportOpen} onOpenChange={setSupportOpen} />
    </div>
  );
}

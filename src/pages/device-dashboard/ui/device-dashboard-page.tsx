import { MoreVertical, Trash2 } from "lucide-react";
import { useState } from "react";
import { useNavigate } from "react-router-dom";

import { useLandlinkDevice } from "@/entities/landlink-device";
import { removeRegisteredDevice } from "@/entities/registered-device";
import { SendMeshForm } from "@/features/send-mesh-message";
import { ROUTES } from "@/shared/config";
import { hapticTick } from "@/shared/lib";
import {
  BackButton,
  Button,
  Drawer,
  DrawerClose,
  DrawerContent,
  DrawerDescription,
  DrawerFooter,
  DrawerHeader,
  DrawerTitle,
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/shared/ui";
import { DeviceTelemetryCard } from "@/widgets/device-telemetry-card";
import { MeshMessageFeed } from "@/widgets/mesh-message-feed";

export function DeviceDashboardPage() {
  const navigate = useNavigate();
  const device = useLandlinkDevice();
  const [confirmOpen, setConfirmOpen] = useState(false);

  const isConnected = device?.status === "connected";

  const handleConfirmRemove = () => {
    if (!device) return;
    hapticTick();
    removeRegisteredDevice(device.deviceId);
    setConfirmOpen(false);
    void navigate(ROUTES.lists, { replace: true });
  };

  return (
    <div className="mx-auto flex h-dvh w-full max-w-[430px] flex-col bg-background">
      <header className="sticky top-0 z-10 flex items-center gap-2 bg-background/90 px-4 ps-1 pt-[max(env(safe-area-inset-top),0.75rem)] pb-3 backdrop-blur supports-backdrop-filter:bg-background/70">
        <BackButton fallback={ROUTES.lists} aria-label="Back to Lists" />
        <h1 className="flex-1 truncate text-base font-medium">
          {device?.name ?? "No device"}
        </h1>
        {device ? (
          <DropdownMenu>
            <DropdownMenuTrigger asChild>
              <button
                type="button"
                aria-label="Device options"
                className="flex size-9 shrink-0 items-center justify-center rounded-md text-muted-foreground hover:bg-muted hover:text-foreground"
                onClick={() => {
                  hapticTick();
                }}
              >
                <MoreVertical className="size-5" aria-hidden="true" />
              </button>
            </DropdownMenuTrigger>
            <DropdownMenuContent>
              <DropdownMenuItem
                variant="destructive"
                onSelect={() => {
                  hapticTick();
                  setConfirmOpen(true);
                }}
              >
                <Trash2 aria-hidden="true" />
                Remove device
              </DropdownMenuItem>
            </DropdownMenuContent>
          </DropdownMenu>
        ) : null}
      </header>
      <main className="flex min-h-0 flex-1 flex-col gap-4 px-4 pt-4 pb-4">
        {isConnected ? (
          <>
            <DeviceTelemetryCard />
            <MeshMessageFeed />
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
      {isConnected ? (
        <div className="bg-background px-4 pt-3 pb-[max(env(safe-area-inset-bottom),0.75rem)]">
          <SendMeshForm />
        </div>
      ) : null}
      <Drawer open={confirmOpen} onOpenChange={setConfirmOpen}>
        <DrawerContent className="pb-[max(env(safe-area-inset-bottom),0.75rem)]">
          <DrawerHeader>
            <DrawerTitle>Remove this device?</DrawerTitle>
            <DrawerDescription>
              {device?.name
                ? `"${device.name}" will be removed from your devices. You can pair it again later.`
                : "This device will be removed from your devices. You can pair it again later."}
            </DrawerDescription>
          </DrawerHeader>
          <DrawerFooter>
            <Button
              variant="destructive"
              size="lg"
              onClick={handleConfirmRemove}
            >
              Remove device
            </Button>
            <DrawerClose asChild>
              <Button
                variant="ghost"
                size="lg"
                onClick={() => {
                  hapticTick();
                }}
              >
                Cancel
              </Button>
            </DrawerClose>
          </DrawerFooter>
        </DrawerContent>
      </Drawer>
    </div>
  );
}

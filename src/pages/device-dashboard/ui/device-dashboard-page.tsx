import { useNavigate } from "react-router-dom";

import { useLandlinkDevice } from "@/entities/landlink-device";
import { SendMeshForm } from "@/features/send-mesh-message";
import { ROUTES } from "@/shared/config";
import { hapticTick } from "@/shared/lib";
import { BackButton, Button } from "@/shared/ui";
import { DeviceTelemetryCard } from "@/widgets/device-telemetry-card";
import { MeshMessageFeed } from "@/widgets/mesh-message-feed";

export function DeviceDashboardPage() {
  const navigate = useNavigate();
  const device = useLandlinkDevice();

  const isConnected = device?.status === "connected";

  return (
    <div className="mx-auto flex h-dvh w-full max-w-[430px] flex-col bg-background">
      <header className="sticky top-0 z-10 flex items-center gap-2 bg-background/90 px-4 ps-1 pt-[max(env(safe-area-inset-top),0.75rem)] pb-3 backdrop-blur supports-backdrop-filter:bg-background/70">
        <BackButton fallback={ROUTES.lists} aria-label="Back to Lists" />
        <h1 className="text-base font-medium">{device?.name ?? "No device"}</h1>
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
    </div>
  );
}

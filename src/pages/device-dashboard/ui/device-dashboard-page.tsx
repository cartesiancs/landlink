import { usePostHog } from "@posthog/react";
import { Info, MoreVertical, Trash2 } from "lucide-react";
import { useState } from "react";
import { useNavigate } from "react-router-dom";

import {
  useLandlinkDevice,
  type ChargeState,
  type DeviceTelemetry,
} from "@/entities/landlink-device";
import { removeRegisteredDevice } from "@/entities/registered-device";
import { SendMeshForm } from "@/features/send-mesh-message";
import { ROUTES } from "@/shared/config";
import { hapticTick } from "@/shared/lib";
import {
  Button,
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
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
import { MeshMessageFeed } from "@/widgets/mesh-message-feed";
import { PageHeader } from "@/widgets/page-header";

function formatLatLon(e7: number): string {
  return (e7 / 1e7).toFixed(6);
}

function chargeStateLabel(state: ChargeState): string {
  const parts: string[] = [];
  if (state.charging) parts.push("Charging");
  if (state.full) parts.push("Full");
  if (state.vbus) parts.push("USB connected");
  if (!state.battPresent) parts.push("No battery");
  if (parts.length === 0) parts.push("On battery");
  return parts.join(" · ");
}

function InfoRow({ label, value }: { label: string; value: string }) {
  return (
    <div className="flex items-baseline justify-between gap-4 py-1.5">
      <span className="text-xs text-muted-foreground">{label}</span>
      <span className="font-mono text-xs text-foreground">{value}</span>
    </div>
  );
}

function TelemetryDialogBody({ telemetry }: { telemetry: DeviceTelemetry }) {
  const pct = Math.max(0, Math.min(100, telemetry.batteryPct));
  const receivedAt = new Date(telemetry.receivedAt).toLocaleString();

  return (
    <div className="flex flex-col gap-4">
      <div>
        <h3 className="mb-1 text-xs font-semibold tracking-wide text-muted-foreground uppercase">
          Battery
        </h3>
        <div className="divide-y divide-border">
          <InfoRow label="Level" value={`${pct.toString()}%`} />
          <InfoRow label="Voltage" value={`${telemetry.batteryMv.toString()} mV`} />
          <InfoRow label="State" value={chargeStateLabel(telemetry.chargeState)} />
        </div>
      </div>

      <div>
        <h3 className="mb-1 text-xs font-semibold tracking-wide text-muted-foreground uppercase">
          GPS
        </h3>
        {telemetry.gps ? (
          <div className="divide-y divide-border">
            <InfoRow label="Latitude" value={formatLatLon(telemetry.gps.latE7)} />
            <InfoRow
              label="Longitude"
              value={formatLatLon(telemetry.gps.lonE7)}
            />
            <InfoRow
              label="Altitude"
              value={`${telemetry.gps.altM.toString()} m`}
            />
            <InfoRow
              label="HDOP"
              value={(telemetry.gps.hdopX10 / 10).toFixed(1)}
            />
            <InfoRow
              label="Speed"
              value={`${(telemetry.gps.speedKmhX10 / 10).toFixed(1)} km/h`}
            />
          </div>
        ) : (
          <p className="text-xs text-muted-foreground">No GPS fix yet.</p>
        )}
      </div>

      <div>
        <h3 className="mb-1 text-xs font-semibold tracking-wide text-muted-foreground uppercase">
          Updated
        </h3>
        <p className="font-mono text-xs text-foreground">{receivedAt}</p>
      </div>
    </div>
  );
}

export function DeviceDashboardPage() {
  const navigate = useNavigate();
  const device = useLandlinkDevice();
  const posthog = usePostHog();
  const [confirmOpen, setConfirmOpen] = useState(false);
  const [infoOpen, setInfoOpen] = useState(false);

  const isConnected = device?.status === "connected";
  const telemetry = device?.telemetry ?? null;

  const handleConfirmRemove = () => {
    if (!device) return;
    hapticTick();
    posthog.capture("device_removed", {
      device_id: device.deviceId,
      device_name: device.name,
    });
    removeRegisteredDevice(device.deviceId);
    setConfirmOpen(false);
    void navigate(ROUTES.lists, { replace: true });
  };

  return (
    <div className="mx-auto flex h-dvh w-full max-w-[430px] flex-col bg-background">
      <PageHeader
        title={device?.name ?? "No device"}
        fallback={ROUTES.lists}
        backLabel="Back to Lists"
      >
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
                onSelect={() => {
                  hapticTick();
                  setInfoOpen(true);
                }}
              >
                <Info aria-hidden="true" />
                Info
              </DropdownMenuItem>
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
      </PageHeader>
      <main className="flex min-h-0 flex-1 flex-col gap-4 px-4 pt-4 pb-4">
        {isConnected ? (
          <MeshMessageFeed />
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
        <div
          className="bg-background px-4 pt-3 transition-[padding-bottom] duration-250 ease-[cubic-bezier(0.32,0.72,0,1)]"
          style={{
            paddingBottom:
              "calc(max(env(safe-area-inset-bottom), 0.75rem) + var(--keyboard-inset, 0px))",
          }}
        >
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
      <Dialog open={infoOpen} onOpenChange={setInfoOpen}>
        <DialogContent>
          <DialogHeader>
            <DialogTitle>Device info</DialogTitle>
            <DialogDescription>
              Latest telemetry snapshot reported by the device.
            </DialogDescription>
          </DialogHeader>
          {telemetry ? (
            <TelemetryDialogBody telemetry={telemetry} />
          ) : (
            <p className="text-sm text-muted-foreground">
              No telemetry received yet.
            </p>
          )}
        </DialogContent>
      </Dialog>
    </div>
  );
}

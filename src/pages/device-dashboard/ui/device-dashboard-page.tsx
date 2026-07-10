import { usePostHog } from "@posthog/react";
import {
  Bluetooth,
  MoreVertical,
  RadioTower,
  Trash2,
  Unplug,
  Wifi,
} from "lucide-react";
import { useState } from "react";
import { useNavigate } from "react-router-dom";

import {
  useLandlinkDevice,
  type ChargeState,
  type DeviceTelemetry,
} from "@/entities/landlink-device";
import {
  findDevice,
  removeRegisteredDevice,
  useRegisteredDevices,
} from "@/entities/registered-device";
import { clearWifiStatus, useWifiStatus } from "@/entities/wifi-status";
import { isRemoteEligible, reconnectController } from "@/features/auto-reconnect";
import { disconnectDevice } from "@/features/disconnect-device";
import { WifiProvisionForm } from "@/features/provision-wifi";
import { ROUTES } from "@/shared/config";
import { cn, hapticTick } from "@/shared/lib";
import {
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
  toast,
} from "@/shared/ui";
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

function TelemetryBlock({ telemetry }: { telemetry: DeviceTelemetry }) {
  const pct = Math.max(0, Math.min(100, telemetry.batteryPct));
  const receivedAt = new Date(telemetry.receivedAt).toLocaleString();

  return (
    <div className="flex flex-col gap-5">
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
  const [wifiOpen, setWifiOpen] = useState(false);

  const registeredDevices = useRegisteredDevices();
  const registered = device
    ? findDevice(registeredDevices, device.deviceId)
    : null;

  const isConnected = device?.status === "connected";
  const isRemote = isConnected && device?.transport === "remote";
  const onBle = device?.transport === "ble";
  const canUseRelay = isRemoteEligible(registered);
  const wifi = useWifiStatus(device?.deviceId ?? null);
  const telemetry = device?.telemetry ?? null;

  const handleConfirmRemove = () => {
    if (!device) return;
    hapticTick();
    posthog.capture("device_removed", {
      device_id: device.deviceId,
      device_name: device.name,
    });
    clearWifiStatus(device.deviceId);
    removeRegisteredDevice(device.deviceId);
    setConfirmOpen(false);
    void navigate(ROUTES.lists, { replace: true });
  };

  const handleDisconnect = () => {
    if (!device) return;
    hapticTick();
    posthog.capture("device_disconnected", {
      device_id: device.deviceId,
      device_name: device.name,
    });
    void disconnectDevice(device.deviceId).then(() => {
      toast.success("Device disconnected.");
    });
  };

  const handleSwitchToRelay = () => {
    if (!device) return;
    hapticTick();
    void reconnectController
      .switchTransport(device.deviceId, device.name, "remote")
      .then((result) => {
        if (result === "remote") {
          toast.success("Connected via Wi-Fi relay.");
        } else {
          toast.error(
            "Couldn't reach the device over the Wi-Fi relay. Check that the relay server is running and the device is online.",
          );
        }
      });
  };

  const handleSwitchToBluetooth = () => {
    if (!device) return;
    hapticTick();
    void reconnectController
      .switchTransport(device.deviceId, device.name, "ble")
      .then((result) => {
        if (result === "ble") {
          toast.success("Connected via Bluetooth.");
        } else {
          toast.error("Couldn't reconnect over Bluetooth.");
        }
      });
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
                  setWifiOpen(true);
                }}
              >
                <Wifi aria-hidden="true" />
                Connect Wi-Fi
              </DropdownMenuItem>
              {onBle && canUseRelay ? (
                <DropdownMenuItem onSelect={handleSwitchToRelay}>
                  <RadioTower aria-hidden="true" />
                  Switch to Wi-Fi relay
                </DropdownMenuItem>
              ) : null}
              {isRemote ? (
                <DropdownMenuItem onSelect={handleSwitchToBluetooth}>
                  <Bluetooth aria-hidden="true" />
                  Switch to Bluetooth
                </DropdownMenuItem>
              ) : null}
              <DropdownMenuItem onSelect={handleDisconnect}>
                <Unplug aria-hidden="true" />
                Disconnect
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
      <main className="flex min-h-0 flex-1 flex-col gap-4 overflow-y-auto px-4 pt-6 pb-[max(env(safe-area-inset-bottom),1.5rem)]">
        {!isConnected ? (
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
        ) : (
          <>
            {isRemote ? (
              <div className="flex items-center gap-2 rounded-xl border border-border bg-card px-3 py-2 text-xs text-muted-foreground">
                <span className="size-2 shrink-0 rounded-full bg-primary" aria-hidden="true" />
                Connected remotely through the relay.
              </div>
            ) : null}
            <div className="flex items-center gap-2 rounded-xl border border-border bg-card px-3 py-2 text-xs text-muted-foreground">
              <Wifi
                className={cn(
                  "size-4 shrink-0",
                  wifi?.connected ? "text-primary" : "text-muted-foreground/50",
                )}
                aria-hidden="true"
              />
              {wifi?.connected
                ? `Wi-Fi connected${wifi.ip ? ` (${wifi.ip})` : ""}`
                : "Wi-Fi not connected"}
            </div>
            {telemetry ? (
              <TelemetryBlock telemetry={telemetry} />
            ) : (
              <p className="text-sm text-muted-foreground">
                No telemetry received yet.
              </p>
            )}
          </>
        )}
        {isConnected ? (
          <div className="mt-6">
            <Button
              variant="outline"
              size="lg"
              className="h-12 w-full"
              onClick={() => {
                hapticTick();
                void navigate(ROUTES.channels);
              }}
            >
              Open channels
            </Button>
          </div>
        ) : null}
      </main>
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
      <Drawer open={wifiOpen} onOpenChange={setWifiOpen}>
        <DrawerContent className="pb-[max(env(safe-area-inset-bottom),0.75rem)]">
          <DrawerHeader>
            <DrawerTitle>Connect Wi-Fi</DrawerTitle>
            <DrawerDescription>
              {wifi?.connected
                ? `Currently connected${wifi.ip ? ` (${wifi.ip})` : ""}. `
                : ""}
              Put this device on Wi-Fi so it stays reachable when Bluetooth is
              out of range.
            </DrawerDescription>
          </DrawerHeader>
          <div className="px-4 pb-2">
            <WifiProvisionForm />
          </div>
        </DrawerContent>
      </Drawer>
    </div>
  );
}

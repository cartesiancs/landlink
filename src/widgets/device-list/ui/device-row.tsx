import { Trash2 } from "lucide-react";
import { useNavigate } from "react-router-dom";

import { useLandlinkDevice } from "@/entities/landlink-device";
import {
  formatLastConnected,
  formatPing,
  removeRegisteredDevice,
  type RegisteredDevice,
} from "@/entities/registered-device";
import { ROUTES } from "@/shared/config";
import { cn, hapticTick } from "@/shared/lib";

import { SignalBars } from "./signal-bars";

type DeviceRowProps = {
  device: RegisteredDevice;
};

export function DeviceRow({ device }: DeviceRowProps) {
  const navigate = useNavigate();
  const liveDevice = useLandlinkDevice();
  const isMock = device.source === "mock";
  const isConnected = device.status === "connected" && device.enabled;
  const isLive =
    isConnected &&
    liveDevice?.deviceId === device.id &&
    liveDevice.status === "connected";

  const goToDashboard = () => {
    if (!isLive) return;
    hapticTick();
    void navigate(ROUTES.deviceDashboard);
  };

  return (
    <li
      className={cn(
        "flex items-center gap-3 rounded-md border border-border px-3 py-3",
        !device.enabled && "opacity-60",
        isLive && "cursor-pointer hover:bg-muted/40",
      )}
      data-source={device.source}
      data-status={device.status}
      onClick={isLive ? goToDashboard : undefined}
    >
      <span
        className={cn(
          "size-2 shrink-0 rounded-full",
          isConnected ? "bg-emerald-500" : "bg-muted-foreground/40",
        )}
        aria-hidden="true"
      />
      <div className="min-w-0 flex-1">
        <div className="flex items-center gap-2">
          <p className="truncate text-sm font-medium">{device.name}</p>
          {isMock && (
            <span className="shrink-0 rounded-sm border border-border px-1.5 py-[1px] text-[10px] uppercase tracking-wide text-muted-foreground">
              Mock
            </span>
          )}
        </div>
        <p className="mt-0.5 truncate text-xs text-muted-foreground">
          {formatPing(device.pingMs)} · {formatLastConnected(device.lastConnectedAt)}
        </p>
      </div>
      <SignalBars signalDbm={device.signalDbm} />
      <button
        type="button"
        aria-label={`Remove ${device.name}`}
        className="ms-1 flex size-8 shrink-0 items-center justify-center rounded-md text-muted-foreground hover:bg-muted hover:text-foreground"
        onClick={(e) => {
          e.stopPropagation();
          hapticTick();
          removeRegisteredDevice(device.id);
        }}
      >
        <Trash2 className="size-4" aria-hidden="true" />
      </button>
    </li>
  );
}

import { Trash2 } from "lucide-react";
import { useNavigate } from "react-router-dom";

import { useLandlinkDevice } from "@/entities/landlink-device";
import { useLoraPeer } from "@/entities/lora-peer";
import {
  formatLastConnected,
  formatPing,
  removeRegisteredDevice,
  type RegisteredDevice,
} from "@/entities/registered-device";
import { useReconnectDevice } from "@/features/auto-reconnect";
import { ROUTES } from "@/shared/config";
import { cn, hapticTick } from "@/shared/lib";

import { SignalBars } from "./signal-bars";

type DeviceRowProps = {
  device: RegisteredDevice;
};

export function DeviceRow({ device }: DeviceRowProps) {
  const navigate = useNavigate();
  const liveDevice = useLandlinkDevice();
  const peer = useLoraPeer(device.nodeId);
  const {
    status: reconnectStatus,
    error: reconnectError,
    reconnect,
  } = useReconnectDevice(device);
  const isMock = device.source === "mock";
  const isConnected = device.status === "connected" && device.enabled;
  const isLive =
    isConnected &&
    liveDevice?.deviceId === device.id &&
    liveDevice.status === "connected";
  const isNearby = !isLive && device.source === "ble" && peer !== null;
  const canReconnect = device.source === "ble" && device.enabled && !isLive;
  const isReconnecting = reconnectStatus === "reconnecting";
  const isClickable = isLive || canReconnect;

  const handleClick = () => {
    if (isLive) {
      hapticTick();
      void navigate(ROUTES.deviceDashboard);
      return;
    }
    if (canReconnect && !isReconnecting) {
      hapticTick();
      void reconnect();
    }
  };

  const subtitle = isReconnecting
    ? "Reconnecting..."
    : reconnectStatus === "error" && reconnectError
    ? reconnectError
    : isNearby && peer
    ? `Nearby via LoRa · ${peer.batteryPct ?? "?"}%`
    : `${formatPing(device.pingMs)} · ${formatLastConnected(
        device.lastConnectedAt,
      )}`;

  // Blue = BLE directly attached (the one and only "primary").
  // Green = LoRa heartbeat seen but not BLE-attached.
  // Amber pulse = reconnect attempt in flight.
  // Gray = registered but neither BLE nor LoRa is reachable.
  const dotClass = isLive
    ? "bg-sky-500"
    : isReconnecting
    ? "animate-pulse bg-amber-400"
    : isNearby
    ? "bg-emerald-500"
    : "bg-muted-foreground/40";

  const signalDbm =
    isNearby && peer?.rssiDbm !== undefined ? peer.rssiDbm : device.signalDbm;

  return (
    <li
      className={cn(
        "flex items-center gap-3 rounded-md border border-border px-3 py-3",
        !device.enabled && "opacity-60",
        isClickable && "cursor-pointer hover:bg-muted/40",
      )}
      data-source={device.source}
      data-status={device.status}
      data-nearby={isNearby ? "true" : undefined}
      onClick={isClickable ? handleClick : undefined}
    >
      <span
        className={cn("size-2 shrink-0 rounded-full", dotClass)}
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
        <p
          className={cn(
            "mt-0.5 truncate text-xs",
            reconnectStatus === "error"
              ? "text-destructive"
              : "text-muted-foreground",
          )}
        >
          {subtitle}
        </p>
      </div>
      <SignalBars signalDbm={signalDbm} />
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

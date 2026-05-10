import { BatteryCharging, BatteryFull, MapPin, Plug, Radio } from "lucide-react";

import {
  useLandlinkDevice,
  type ChargeState,
  type DeviceTelemetry,
  type GpsFix,
} from "@/entities/landlink-device";

function ChargeIcon({ chargeState }: { chargeState: ChargeState }) {
  if (chargeState.charging) {
    return (
      <BatteryCharging
        className="size-4 text-emerald-500"
        aria-label="Charging"
      />
    );
  }
  if (chargeState.full) {
    return <BatteryFull className="size-4 text-emerald-500" aria-label="Full" />;
  }
  if (chargeState.vbus) {
    return <Plug className="size-4 text-muted-foreground" aria-label="USB power" />;
  }
  return null;
}

function formatLatLon(e7: number): string {
  return (e7 / 1e7).toFixed(6);
}

function GpsBlock({ gps }: { gps: GpsFix | null }) {
  if (!gps) {
    return (
      <div className="flex items-center gap-2 text-sm text-muted-foreground">
        <Radio className="size-4" aria-hidden="true" />
        <span>No GPS fix</span>
      </div>
    );
  }
  return (
    <div className="flex items-start gap-2 text-sm">
      <MapPin
        className="mt-0.5 size-4 shrink-0 text-muted-foreground"
        aria-hidden="true"
      />
      <div className="font-mono">
        <div>
          {formatLatLon(gps.latE7)}, {formatLatLon(gps.lonE7)}
        </div>
        <div className="text-xs text-muted-foreground">
          alt {gps.altM} m · HDOP {(gps.hdopX10 / 10).toFixed(1)}
          {gps.speedKmhX10 > 0 ? ` · ${(gps.speedKmhX10 / 10).toFixed(1)} km/h` : ""}
        </div>
      </div>
    </div>
  );
}

function BatteryBlock({ telemetry }: { telemetry: DeviceTelemetry }) {
  const pct = Math.max(0, Math.min(100, telemetry.batteryPct));
  return (
    <div className="flex flex-col gap-1">
      <div className="flex items-center justify-between text-sm">
        <div className="flex items-center gap-2">
          <span className="font-medium">{pct}%</span>
          <span className="text-xs text-muted-foreground">
            {telemetry.batteryMv} mV
          </span>
        </div>
        <ChargeIcon chargeState={telemetry.chargeState} />
      </div>
      <div className="h-1.5 w-full overflow-hidden rounded-full bg-muted">
        <div
          className="h-full bg-foreground transition-[width] duration-500"
          style={{ width: `${pct}%` }}
        />
      </div>
    </div>
  );
}

export function DeviceTelemetryCard() {
  const device = useLandlinkDevice();
  const telemetry = device?.telemetry ?? null;

  return (
    <section className="flex flex-col gap-3 rounded-lg border border-border bg-card p-4">
      <h2 className="text-sm font-semibold tracking-tight">Telemetry</h2>
      {telemetry ? (
        <>
          <BatteryBlock telemetry={telemetry} />
          <div className="border-t border-border pt-3">
            <GpsBlock gps={telemetry.gps} />
          </div>
        </>
      ) : (
        <p className="text-sm text-muted-foreground">Waiting for telemetry…</p>
      )}
    </section>
  );
}

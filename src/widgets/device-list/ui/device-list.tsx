import { useRegisteredDevices } from "@/entities/registered-device";

import { DeviceRow } from "./device-row";

export function DeviceList() {
  const devices = useRegisteredDevices();

  if (devices.length === 0) {
    return (
      <div className="flex flex-col items-center gap-2 rounded-md border border-dashed border-border px-4 py-10 text-center">
        <p className="text-sm font-medium">No devices yet</p>
        <p className="text-xs text-muted-foreground">
          Tap "Connect new device" to pair your first Landlink.
        </p>
      </div>
    );
  }

  return (
    <ul className="flex flex-col gap-2">
      {devices.map((device) => (
        <DeviceRow key={device.id} device={device} />
      ))}
    </ul>
  );
}

import { useEffect } from "react";

import {
  landlinkChannelList,
  onLandlinkEvt,
  parseChannelResult,
  useLandlinkDevice,
  type DeviceChannel,
  type LandlinkEvtFrame,
} from "@/entities/landlink-device";
import {
  removeChannel,
  setChannels,
  upsertChannel,
  type Channel,
} from "@/entities/meshtastic-channel";
import { findDevice, useRegisteredDevices } from "@/entities/registered-device";
import { Opcode } from "@/shared/protocol";

function toChannel(d: DeviceChannel): Channel {
  return {
    index: d.index,
    name: d.name,
    psk: d.psk,
    role: d.role,
    createdAt: 0,
  };
}

// Bridges the firmware's channel registry (read via CHANNEL_LIST /
// CHANNEL_RESULT BLE opcodes) into the meshtastic-channel store. Only runs
// for our Landlink-family devices; stock Meshtastic devices populate the
// same store through their own FromRadio.channel adapter, so this hook
// stays out of the way for that family.
export function useSyncDeviceChannels(): void {
  const device = useLandlinkDevice();
  const registeredDevices = useRegisteredDevices();

  // Read the connection identity down to primitives so the effect doesn't
  // re-run on every store mutation (telemetry, FSM bumps, EVT frames, etc.).
  // The landlink-device store re-creates its snapshot object on every
  // change; without this projection the effect would cancel the in-flight
  // sync repeatedly and setChannels would never fire.
  const deviceId = device?.deviceId ?? null;
  const status = device?.status ?? null;
  const registered = deviceId
    ? findDevice(registeredDevices, deviceId)
    : null;
  const protocolFamily = registered?.protocol ?? null;

  useEffect(() => {
    if (deviceId === null || status !== "connected") return;
    if (protocolFamily === "meshtastic") return;

    let cancelled = false;

    landlinkChannelList()
      .then((channels) => {
        if (cancelled) return;
        setChannels(deviceId, channels.map(toChannel));
      })
      .catch((err: unknown) => {
        console.warn("[channels] initial sync failed", err);
      });

    const unsub = onLandlinkEvt((frame: LandlinkEvtFrame) => {
      if (frame.opcode !== Opcode.CHANNEL_RESULT) return;
      const result = parseChannelResult(frame.payload);
      if (!result) return;
      if (result.kind === "delete") {
        removeChannel(deviceId, result.index);
      } else {
        upsertChannel(deviceId, toChannel(result.channel));
      }
    });

    return () => {
      cancelled = true;
      unsub();
    };
  }, [deviceId, status, protocolFamily]);
}

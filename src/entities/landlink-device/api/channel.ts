// BLE helpers for the firmware's CHANNEL_* opcodes. Both Landlink-native and
// Meshtastic-compatible firmware modes share the same channel registry, so
// these helpers work regardless of the device's active protocol mode.
//
// CHANNEL_LIST_RESULT streams one EVT per occupied slot (mirroring the
// WIFI_SCAN_RESULT pattern). The list helper installs a temporary listener
// on the EVT bus, collects results, and resolves after a short quiet window.
//
// This module lives in the landlink-device entity because it depends on the
// connected-device session (sendLandlinkCommand + the EVT subscription).
// Consumers convert the wire-level DeviceChannel into richer per-app types
// (e.g. meshtastic-channel's Channel) at the feature layer.

import { Opcode, TlvTag, decodeTlvs } from "@/shared/protocol";

import { onLandlinkEvt, sendLandlinkCommand, type LandlinkEvtFrame } from "./client";

const LIST_QUIET_WINDOW_MS = 600;

export type DeviceChannelRole = "primary" | "secondary";

// Wire-level snapshot of one channel slot on the firmware. PSK may be empty
// when the BLE session is unauthed — the firmware withholds it in that case.
export type DeviceChannel = {
  index: number;
  name: string;
  psk: Uint8Array;
  role: DeviceChannelRole;
};

export type DeviceChannelResult =
  | { kind: "upsert"; channel: DeviceChannel }
  | { kind: "delete"; index: number };

function readU8(value: Uint8Array): number | null {
  if (value.byteLength !== 1) return null;
  return value[0] ?? null;
}

function roleFromByte(value: number | null): DeviceChannelRole | null {
  if (value === 0) return "primary";
  if (value === 1) return "secondary";
  return null;
}

function parseChannel(payload: Uint8Array): DeviceChannel | null {
  let index: number | null = null;
  let name: string | null = null;
  let psk: Uint8Array | null = null;
  let role: DeviceChannelRole | null = null;
  for (const t of decodeTlvs(payload)) {
    if (t.tag === TlvTag.CHANNEL_INDEX) {
      index = readU8(t.value);
    } else if (t.tag === TlvTag.CHANNEL_NAME) {
      name = new TextDecoder().decode(t.value);
    } else if (t.tag === TlvTag.CHANNEL_PSK) {
      psk = new Uint8Array(t.value);
    } else if (t.tag === TlvTag.CHANNEL_ROLE) {
      role = roleFromByte(readU8(t.value));
    }
  }
  if (index === null || name === null || role === null) return null;
  return { index, name, psk: psk ?? new Uint8Array(0), role };
}

export function parseChannelResult(payload: Uint8Array): DeviceChannelResult | null {
  let index: number | null = null;
  let sawName = false;
  let sawPsk = false;
  let sawRole = false;
  for (const t of decodeTlvs(payload)) {
    if (t.tag === TlvTag.CHANNEL_INDEX) index = readU8(t.value);
    else if (t.tag === TlvTag.CHANNEL_NAME) sawName = true;
    else if (t.tag === TlvTag.CHANNEL_PSK) sawPsk = true;
    else if (t.tag === TlvTag.CHANNEL_ROLE) sawRole = true;
  }
  if (index === null) return null;
  if (!sawName && !sawPsk && !sawRole) {
    return { kind: "delete", index };
  }
  const channel = parseChannel(payload);
  if (!channel) return null;
  return { kind: "upsert", channel };
}

export async function landlinkChannelList(): Promise<readonly DeviceChannel[]> {
  return new Promise<readonly DeviceChannel[]>((resolve, reject) => {
    const collected: DeviceChannel[] = [];
    let timer: number | null = null;
    const finalize = () => {
      unsubscribe();
      collected.sort((a, b) => a.index - b.index);
      resolve(collected);
    };
    const unsubscribe = onLandlinkEvt((frame: LandlinkEvtFrame) => {
      if (frame.opcode !== Opcode.CHANNEL_LIST_RESULT) return;
      const channel = parseChannel(frame.payload);
      if (channel) collected.push(channel);
      if (timer !== null) window.clearTimeout(timer);
      timer = window.setTimeout(finalize, LIST_QUIET_WINDOW_MS);
    });
    sendLandlinkCommand(Opcode.CHANNEL_LIST).then(() => {
      // No EVTs arrived synchronously; wait the quiet window then resolve
      // (empty if the device truly has nothing, or with whatever's
      // collected by then).
      timer ??= window.setTimeout(finalize, LIST_QUIET_WINDOW_MS);
    }).catch((err: unknown) => {
      if (timer !== null) window.clearTimeout(timer);
      unsubscribe();
      reject(err instanceof Error ? err : new Error(String(err)));
    });
  });
}

export async function landlinkChannelSet(
  index: number,
  name: string,
  psk: Uint8Array,
  role: DeviceChannelRole = index === 0 ? "primary" : "secondary",
): Promise<void> {
  const nameBytes = new TextEncoder().encode(name);
  await sendLandlinkCommand(Opcode.CHANNEL_SET, [
    { tag: TlvTag.CHANNEL_INDEX, value: Uint8Array.of(index) },
    { tag: TlvTag.CHANNEL_NAME, value: nameBytes },
    { tag: TlvTag.CHANNEL_PSK, value: psk },
    {
      tag: TlvTag.CHANNEL_ROLE,
      value: Uint8Array.of(role === "primary" ? 0 : 1),
    },
  ]);
}

export async function landlinkChannelDelete(index: number): Promise<void> {
  await sendLandlinkCommand(Opcode.CHANNEL_DELETE, [
    { tag: TlvTag.CHANNEL_INDEX, value: Uint8Array.of(index) },
  ]);
}

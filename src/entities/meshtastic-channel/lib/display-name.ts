import type { Channel } from "../model/types";
import { PRIMARY_NAME } from "./defaults";

// The primary channel's wire-level name on Meshtastic is the LoRa preset
// ("LongFast", "ShortFast", etc.) — the channel hash is xor(name)^xor(psk),
// so renaming it on the device would silently break interop with stock
// Meshtastic peers that still hash against the preset name. Keep the wire
// name in the channel record and render "Primary" in the UI for any
// primary-role channel so the user sees the friendly label.
export function displayChannelName(channel: Channel): string {
  return channel.role === "primary" ? PRIMARY_NAME : channel.name;
}

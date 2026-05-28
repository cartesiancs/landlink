// Channel is the per-device PSK-encrypted group concept from Meshtastic. A
// device has up to 8 channels (index 0..7). Index 0 = Primary and always
// exists; secondary channels (1..7) are user-created. PSK is 32 bytes
// (AES-256-CTR key). In STEP 1 we synthesize Primary locally and persist
// secondary channels in localStorage; STEP 2 will swap the data source to
// the connected device via Meshtastic FromRadio.channel messages.

export const MAX_CHANNEL_INDEX = 7;
export const NUM_CHANNELS = 8;

export type ChannelRole = "primary" | "secondary";

export type Channel = {
  index: number;
  name: string;
  psk: Uint8Array;
  role: ChannelRole;
  createdAt: number;
};

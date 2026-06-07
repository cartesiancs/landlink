import { useCallback, useState } from "react";

import {
  appendOutgoingPending,
  sendLandlinkCommand,
  trackPendingChat,
  useLandlinkDevice,
} from "@/entities/landlink-device";
import { sendMeshtasticText } from "@/entities/meshtastic-device";
import {
  findDevice,
  useRegisteredDevices,
} from "@/entities/registered-device";
import { MeshKind, Opcode, TlvTag, type Tlv } from "@/shared/protocol";

export type SendMeshMessageStatus = "idle" | "sending" | "sent" | "error";

const MAX_TEXT_BYTES = 200;

// 8-char hex nodeId → u32. Inverse of the nodeIdHex(num) used by the
// Meshtastic adapter when surfacing senders to the UI.
function nodeIdHexToNum(hex: string): number | null {
  if (hex.length !== 8) return null;
  if (!/^[0-9a-f]{8}$/iu.test(hex)) return null;
  const n = parseInt(hex, 16);
  return Number.isFinite(n) ? n >>> 0 : null;
}

export type SendOptions = {
  // Hex nodeId of the recipient. When set, the message is addressed as a
  // unicast — firmware-side PKI (X25519+AES-CCM) kicks in transparently
  // when the recipient's public_key has been heard via NodeInfo. Without
  // a recipient key the firmware falls back to channel PSK.
  recipientNodeId?: string;
};

export function useSendMeshMessage(channelIndex = 0) {
  const [status, setStatus] = useState<SendMeshMessageStatus>("idle");
  const [error, setError] = useState<string | null>(null);
  const device = useLandlinkDevice();
  const registeredDevices = useRegisteredDevices();
  const registered = device
    ? findDevice(registeredDevices, device.deviceId)
    : null;
  // Connected-device protocol family (Landlink TLV vs Meshtastic). Falls
  // back to "landlink" for legacy registered entries that predate the field.
  const adapter = registered?.protocol ?? "landlink";

  const send = useCallback(
    async (text: string, opts: SendOptions = {}): Promise<boolean> => {
    const trimmed = text.trim();
    if (trimmed.length === 0) {
      setStatus("error");
      setError("Message is empty");
      return false;
    }

    const encoded = new TextEncoder().encode(trimmed);
    if (encoded.byteLength > MAX_TEXT_BYTES) {
      setStatus("error");
      setError(`Message exceeds ${MAX_TEXT_BYTES} bytes`);
      return false;
    }

    setStatus("sending");
    setError(null);

    if (adapter === "meshtastic") {
      try {
        // Firmware owns the X25519 keypair and runs PKI encryption when the
        // recipient's public_key is cached (STEP 3+). The app only forwards
        // plaintext + destination — picking PSK vs PKI happens device-side.
        if (opts.recipientNodeId) {
          const destNum = nodeIdHexToNum(opts.recipientNodeId);
          if (destNum === null) throw new Error("Invalid recipient node id");
          await sendMeshtasticText(trimmed, channelIndex, { dest: destNum });
        } else {
          await sendMeshtasticText(trimmed, channelIndex);
        }
        // sendMeshtasticText optimistically appends the outgoing message into
        // the local feed (stock Meshtastic firmware does not echo it back via
        // FromRadio), so the UI updates without waiting on a roundtrip.
        setStatus("sent");
        return true;
      } catch (err) {
        setStatus("error");
        setError(err instanceof Error ? err.message : "Send failed");
        return false;
      }
    }

    // Landlink path: TLV/opcode framing. The firmware always runs in
    // Meshtastic-compatible mode (LongFast), so the per-channel key on
    // header byte 13 selects the slot — any 0..7 index that resolves to a
    // configured channel works.
    const tlvs: Tlv[] = [
      { tag: TlvTag.KIND, value: Uint8Array.of(MeshKind.CHAT_TEXT) },
      { tag: TlvTag.CHANNEL_INDEX, value: Uint8Array.of(channelIndex) },
      { tag: TlvTag.CHAT_TEXT, value: encoded },
    ];

    try {
      // Firmware surfaces MESH_SEND_RESULT with the assigned pkt_id, so the
      // host can wait for the Meshtastic Routing ACK (matching request_id)
      // and flip the message to "delivered". Retry is disabled — there's no
      // on-wire RETRY_PKT_ID in Meshtastic and duplicate sends would render
      // as new messages on the receiver. The pre-seq hook registers the
      // pending entry synchronously, before the BLE write, because
      // MESH_SEND_RESULT can race past the await on a fast link.
      await sendLandlinkCommand(Opcode.MESH_SEND, tlvs, (seq) => {
        appendOutgoingPending(trimmed, seq, channelIndex);
        trackPendingChat(seq, trimmed, encoded, { noRetry: true });
      });
      setStatus("sent");
      return true;
    } catch (err) {
      setStatus("error");
      setError(err instanceof Error ? err.message : "Send failed");
      return false;
    }
  }, [adapter, channelIndex]);

  const reset = useCallback(() => {
    setStatus("idle");
    setError(null);
  }, []);

  return {
    status,
    error,
    send,
    reset,
    maxBytes: MAX_TEXT_BYTES,
    adapter,
    channelIndex,
  };
}

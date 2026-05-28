import { useCallback, useState } from "react";

import {
  appendOutgoingMessage,
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

export function useSendMeshMessage(channelIndex = 0) {
  const [status, setStatus] = useState<SendMeshMessageStatus>("idle");
  const [error, setError] = useState<string | null>(null);
  const device = useLandlinkDevice();
  const protocolMode = device?.protocol ?? null;
  const registeredDevices = useRegisteredDevices();
  const registered = device
    ? findDevice(registeredDevices, device.deviceId)
    : null;
  // Connected-device protocol family (Landlink TLV vs Meshtastic). Falls
  // back to "landlink" for legacy registered entries that predate the field.
  const adapter = registered?.protocol ?? "landlink";

  const send = useCallback(async (text: string): Promise<boolean> => {
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
        await sendMeshtasticText(trimmed, channelIndex);
        // The device echoes our packet back via FromRadio so the message
        // shows up in the feed without an optimistic append (which would
        // double-render). See sendMeshtasticText's comment for context.
        setStatus("sent");
        return true;
      } catch (err) {
        setStatus("error");
        setError(err instanceof Error ? err.message : "Send failed");
        return false;
      }
    }

    // Landlink path: TLV/opcode framing. The firmware's channel registry
    // selects the per-channel key for both Landlink-native (trial decrypt
    // on RX) and Meshtastic-compatible (header byte 13) frames, so any
    // 0..7 index that resolves to a configured slot works.
    const tlvs: Tlv[] = [
      { tag: TlvTag.KIND, value: Uint8Array.of(MeshKind.CHAT_TEXT) },
      { tag: TlvTag.CHANNEL_INDEX, value: Uint8Array.of(channelIndex) },
      { tag: TlvTag.CHAT_TEXT, value: encoded },
    ];

    try {
      // Both protocols surface MESH_SEND_RESULT with the assigned pkt_id, so
      // the host can wait for an ACK (landlink: KIND=ACK frame; meshtastic:
      // Routing reply with matching request_id) and flip the message to
      // "delivered". Meshtastic mode disables retry — there's no on-wire
      // equivalent of RETRY_PKT_ID and duplicate sends would be rendered as
      // new messages on the receiver. The pre-seq hook registers the pending
      // entry synchronously, before the BLE write, because MESH_SEND_RESULT
      // can race past the await on a fast link.
      let registeredPending = false;
      await sendLandlinkCommand(Opcode.MESH_SEND, tlvs, (seq) => {
        if (protocolMode !== null) {
          appendOutgoingPending(trimmed, seq, channelIndex);
          trackPendingChat(seq, trimmed, encoded, {
            noRetry: protocolMode === 1,
          });
          registeredPending = true;
        }
      });
      if (!registeredPending) {
        appendOutgoingMessage(trimmed, channelIndex);
      }
      setStatus("sent");
      return true;
    } catch (err) {
      setStatus("error");
      setError(err instanceof Error ? err.message : "Send failed");
      return false;
    }
  }, [adapter, channelIndex, protocolMode]);

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

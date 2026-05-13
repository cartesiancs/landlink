import { useCallback, useState } from "react";

import {
  appendOutgoingMessage,
  appendOutgoingPending,
  sendLandlinkCommand,
  trackPendingChat,
  useLandlinkDevice,
} from "@/entities/landlink-device";
import { MeshKind, Opcode, TlvTag, type Tlv } from "@/shared/protocol";

export type SendMeshMessageStatus = "idle" | "sending" | "sent" | "error";

const MAX_TEXT_BYTES = 200;

export function useSendMeshMessage() {
  const [status, setStatus] = useState<SendMeshMessageStatus>("idle");
  const [error, setError] = useState<string | null>(null);
  const device = useLandlinkDevice();
  const protocolMode = device?.protocol ?? null;

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

    const tlvs: Tlv[] = [
      { tag: TlvTag.KIND, value: Uint8Array.of(MeshKind.CHAT_TEXT) },
      { tag: TlvTag.CHAT_TEXT, value: encoded },
    ];

    setStatus("sending");
    setError(null);
    try {
      // ACK/retry is a landlink-only protocol feature. Meshtastic mode is
      // fire-and-forget and keeps its existing UX. The pre-seq hook registers
      // the pending entry synchronously, before the BLE write — necessary
      // because MESH_SEND_RESULT can race past the await on a fast link.
      let registered = false;
      await sendLandlinkCommand(Opcode.MESH_SEND, tlvs, (seq) => {
        if (protocolMode === 0) {
          appendOutgoingPending(trimmed, seq);
          trackPendingChat(seq, trimmed, encoded);
          registered = true;
        }
      });
      if (!registered) {
        appendOutgoingMessage(trimmed);
      }
      setStatus("sent");
      return true;
    } catch (err) {
      setStatus("error");
      setError(err instanceof Error ? err.message : "Send failed");
      return false;
    }
  }, [protocolMode]);

  const reset = useCallback(() => {
    setStatus("idle");
    setError(null);
  }, []);

  return { status, error, send, reset, maxBytes: MAX_TEXT_BYTES };
}

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
import { nodeNumToBytesLE } from "@/shared/lib";
import { MeshKind, Opcode, TlvTag, type Tlv } from "@/shared/protocol";

export type SendMeshMessageStatus = "idle" | "sending" | "sent" | "error";

const MAX_TEXT_BYTES = 200;

export type SendOptions = {
  // Numeric node id of the recipient. When set, the message is addressed as a
  // unicast: firmware sets FlagUnicast on the Landlink wire, sets
  // MeshPacket.to=recipientNodeNum on Meshtastic, and PKI (X25519+AES-CCM)
  // kicks in transparently when the recipient's public_key has been heard via
  // NodeInfo. Without a recipient the message broadcasts on the channel.
  recipientNodeNum?: number;
};

export function useSendMeshMessage(channelIndex = 0) {
  const [status, setStatus] = useState<SendMeshMessageStatus>("idle");
  const [error, setError] = useState<string | null>(null);
  const device = useLandlinkDevice();
  const registeredDevices = useRegisteredDevices();
  const registered = device
    ? findDevice(registeredDevices, device.deviceId)
    : null;
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
        if (opts.recipientNodeNum !== undefined) {
          await sendMeshtasticText(trimmed, channelIndex, {
            dest: opts.recipientNodeNum,
          });
        } else {
          await sendMeshtasticText(trimmed, channelIndex);
        }
        setStatus("sent");
        return true;
      } catch (err) {
        setStatus("error");
        setError(err instanceof Error ? err.message : "Send failed");
        return false;
      }
    }

    // Landlink path: TLV/opcode framing. The firmware always runs in
    // Meshtastic-compatible mode (LongFast). When a recipient is set, push a
    // NODE_ID TLV with the destination as 4 LE bytes; firmware reads this as
    // the unicast destination and the router stamps FlagUnicast on the wire.
    const tlvs: Tlv[] = [
      { tag: TlvTag.KIND, value: Uint8Array.of(MeshKind.CHAT_TEXT) },
      { tag: TlvTag.CHANNEL_INDEX, value: Uint8Array.of(channelIndex) },
    ];
    if (opts.recipientNodeNum !== undefined) {
      tlvs.push({
        tag: TlvTag.NODE_ID,
        value: nodeNumToBytesLE(opts.recipientNodeNum),
      });
    }
    tlvs.push({ tag: TlvTag.CHAT_TEXT, value: encoded });

    try {
      const recipient = opts.recipientNodeNum;
      await sendLandlinkCommand(Opcode.MESH_SEND, tlvs, (seq) => {
        appendOutgoingPending(
          trimmed,
          seq,
          channelIndex,
          recipient !== undefined ? { recipientNodeNum: recipient } : {},
        );
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

import { useCallback, useState } from "react";

import {
  appendOutgoingPending,
  sendLandlinkCommand,
  trackPendingChat,
  useLandlinkDevice,
} from "@/entities/landlink-device";
import {
  requestMeshtasticNodeInfo,
  sendMeshtasticText,
} from "@/entities/meshtastic-device";
import {
  findPublicKey,
  subscribePublicKeys,
} from "@/entities/meshtastic-pki";
import {
  findDevice,
  useRegisteredDevices,
  type RegisteredDeviceProtocol,
} from "@/entities/registered-device";
import { nodeNumToBytesLE } from "@/shared/lib";
import { MeshKind, Opcode, TlvTag, type Tlv } from "@/shared/protocol";

export type SendMeshMessageStatus =
  | "idle"
  | "requesting-key"
  | "sending"
  | "sent"
  | "error";

const MAX_TEXT_BYTES = 200;
// How long to wait for a NodeInfo reply (with public_key) after firing an
// explicit request. NodeInfoModule typically responds within ~1s on a quiet
// channel but can be slower under load or via relay; 10s balances UX with
// realistic mesh latency.
const KEY_LEARN_TIMEOUT_MS = 10_000;

export type SendOptions = {
  // Numeric node id of the recipient. When set, the message is addressed as a
  // unicast: firmware sets FlagUnicast on the Landlink wire, sets
  // MeshPacket.to=recipientNodeNum on Meshtastic, and PKI (X25519+AES-CCM)
  // kicks in transparently when the recipient's public_key has been heard via
  // NodeInfo. Without a recipient the message broadcasts on the channel.
  recipientNodeNum?: number;
  // When true, skip the PKI bootstrap step and send immediately even if the
  // recipient's public_key is unknown. Used by the UI fallback dialog after
  // a NodeInfo request times out, so the user can opt into PSK delivery.
  skipPkiBootstrap?: boolean;
};

export const SEND_MESH_ERROR_KEY_TIMEOUT = "PKI_KEY_TIMEOUT";
// Raised when sending a unicast on the Landlink adapter and the recipient's
// public_key is not in our PKI store. We do not attempt an active NodeInfo
// request here because Landlink's BLE protocol does not currently expose a
// way for the host to ask the firmware to send one (the Landlink wire is
// TLV-based, not a direct Meshtastic ToRadio pipe). The UI catches this
// error and offers an adapter-appropriate PSK fallback dialog. Once the
// peer's next periodic NodeInfo broadcast (~15 min) arrives, this code
// path is bypassed.
export const SEND_MESH_ERROR_KEY_UNKNOWN = "PKI_KEY_UNKNOWN";

function waitForKey(
  recipientNodeNum: number,
  timeoutMs: number,
): Promise<boolean> {
  return new Promise((resolve) => {
    if (findPublicKey(recipientNodeNum) !== null) {
      resolve(true);
      return;
    }
    let settled = false;
    const unsubscribe = subscribePublicKeys(() => {
      if (settled) return;
      if (findPublicKey(recipientNodeNum) !== null) {
        settled = true;
        clearTimeout(timer);
        unsubscribe();
        resolve(true);
      }
    });
    const timer = setTimeout(() => {
      if (settled) return;
      settled = true;
      unsubscribe();
      resolve(false);
    }, timeoutMs);
  });
}

export type UseSendMeshMessageResult = {
  status: SendMeshMessageStatus;
  error: string | null;
  send: (text: string, opts?: SendOptions) => Promise<boolean>;
  reset: () => void;
  maxBytes: number;
  adapter: RegisteredDeviceProtocol;
  channelIndex: number;
};

export function useSendMeshMessage(channelIndex = 0): UseSendMeshMessageResult {
  const [status, setStatus] = useState<SendMeshMessageStatus>("idle");
  const [error, setError] = useState<string | null>(null);
  const device = useLandlinkDevice();
  const registeredDevices = useRegisteredDevices();
  const registered = device
    ? findDevice(registeredDevices, device.deviceId)
    : null;
  const adapter: RegisteredDeviceProtocol = registered?.protocol ?? "landlink";

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

    setError(null);

    if (adapter === "meshtastic") {
      // B3 bootstrap: when a unicast DM targets a peer whose X25519 public_key
      // is not yet in the PKI store, the firmware would reject the send with
      // PKI_SEND_FAIL_PUBLIC_KEY. Fire an explicit NodeInfo request first
      // and wait up to KEY_LEARN_TIMEOUT_MS for the reply before sending.
      // The UI fallback dialog routes the user back here with
      // skipPkiBootstrap=true if they explicitly opt into PSK delivery.
      const needsBootstrap =
        opts.recipientNodeNum !== undefined &&
        !opts.skipPkiBootstrap &&
        findPublicKey(opts.recipientNodeNum) === null;
      if (needsBootstrap && opts.recipientNodeNum !== undefined) {
        const target = opts.recipientNodeNum;
        setStatus("requesting-key");
        try {
          await requestMeshtasticNodeInfo(target);
        } catch (err) {
          setStatus("error");
          setError(err instanceof Error ? err.message : "Key request failed");
          return false;
        }
        const learned = await waitForKey(target, KEY_LEARN_TIMEOUT_MS);
        if (!learned) {
          setStatus("error");
          setError(SEND_MESH_ERROR_KEY_TIMEOUT);
          return false;
        }
      }
      setStatus("sending");
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

    // Landlink unicast + unknown key: surface a fallback choice to the user
    // instead of letting the firmware silently degrade to channel PSK. The
    // user's host store would still show "PKI" intent while the wire goes
    // out as PSK, which is misleading. Forcing the explicit dialog also
    // matches the Meshtastic-adapter behavior so the UX is consistent.
    if (
      opts.recipientNodeNum !== undefined &&
      !opts.skipPkiBootstrap &&
      findPublicKey(opts.recipientNodeNum) === null
    ) {
      setStatus("error");
      setError(SEND_MESH_ERROR_KEY_UNKNOWN);
      return false;
    }

    setStatus("sending");

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

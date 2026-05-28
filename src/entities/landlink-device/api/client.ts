import {
  findDevice,
  getRegisteredDevices,
  updateRegisteredDevice,
} from "@/entities/registered-device";
import {
  disconnectLandlinkDevice,
  onLandlinkDisconnect,
  readCharacteristic,
  startNotifications,
  writeCharacteristic,
} from "@/shared/api";
import {
  decodeFrame,
  decodeTlvs,
  encodeFrame,
  encodeTlvs,
  LANDLINK_CHARACTERISTIC,
  LANDLINK_SERVICE_UUID,
  Opcode,
  TlvTag,
  type FsmStateValue,
  type OpcodeValue,
  type Tlv,
} from "@/shared/protocol";

import { parseLandlinkInfo } from "../lib/parse-info";
import { parseMeshRecv } from "../lib/parse-mesh-recv";
import { parseTelemetry } from "../lib/parse-telemetry";
import {
  appendMessage,
  failAllOutgoingPending,
  getState,
  setConnected,
  setConnecting,
  setDisconnected,
  setFsmState,
  setInfo,
  setLastEvtFrame,
  setProtocol,
  setTelemetry,
  subscribe as subscribeStore,
  type MeshMessage,
  type ProtocolMode,
} from "../model/store";
import {
  attachPktId,
  cancelAll as cancelAllRetries,
  onAck,
  setRetrySender,
  trackPending,
  type TrackPendingOptions,
} from "../model/retry-tracker";

let seqCounter = 0;
function nextSeq(): number {
  seqCounter = (seqCounter + 1) & 0xff;
  return seqCounter;
}

// Small LRU over (senderNodeId, pktId) to deduplicate incoming chat that
// firmware may forward more than once (rare: a duplicate flag would normally
// suppress the BLE notify, but BLE retries on the host side, or future kinds
// that route through this path, can race). Acts as second-line defense.
const RECENT_CHAT_CACHE = 32;
const recentChats: string[] = [];
function chatSeen(key: string): boolean {
  const idx = recentChats.indexOf(key);
  if (idx !== -1) return true;
  recentChats.push(key);
  if (recentChats.length > RECENT_CHAT_CACHE) recentChats.shift();
  return false;
}

function readU32LE(bytes: Uint8Array): number | null {
  if (bytes.byteLength !== 4) return null;
  const b0 = bytes[0] ?? 0;
  const b1 = bytes[1] ?? 0;
  const b2 = bytes[2] ?? 0;
  const b3 = bytes[3] ?? 0;
  return (b0 | (b1 << 8) | (b2 << 16) | (b3 << 24)) >>> 0;
}

let activeDeviceId: string | null = null;
let activeStoppers: (() => Promise<void>)[] = [];
let activeUnsubDisconnect: (() => void) | null = null;

export type PeerFoundFrame = { seq: number; payload: Uint8Array };
type PeerFoundHandler = (frame: PeerFoundFrame) => void;
const peerFoundHandlers = new Set<PeerFoundHandler>();

export function onLandlinkPeerFound(handler: PeerFoundHandler): () => void {
  peerFoundHandlers.add(handler);
  return () => {
    peerFoundHandlers.delete(handler);
  };
}

// Generic EVT bus. Fires for every decoded EVT frame after the client's
// built-in handlers (telemetry, mesh-recv, ack-tracking, etc.) have run, so
// downstream consumers can subscribe to opcodes the client does not own
// (e.g. CHANNEL_LIST_RESULT, CHANNEL_RESULT) without each of them needing
// their own characteristic subscription.
export type LandlinkEvtFrame = {
  opcode: number;
  seq: number;
  payload: Uint8Array;
};
type EvtHandler = (frame: LandlinkEvtFrame) => void;
const evtHandlers = new Set<EvtHandler>();

export function onLandlinkEvt(handler: EvtHandler): () => void {
  evtHandlers.add(handler);
  return () => {
    evtHandlers.delete(handler);
  };
}

function broadcastEvt(frame: LandlinkEvtFrame): void {
  for (const h of evtHandlers) {
    try {
      h(frame);
    } catch {
      // handlers must not break the EVT stream
    }
  }
}

async function runStoppers(): Promise<void> {
  const stoppers = activeStoppers;
  activeStoppers = [];
  for (const stop of stoppers) {
    try {
      await stop();
    } catch {
      // best effort
    }
  }
}

function clearActive(): void {
  activeUnsubDisconnect?.();
  activeUnsubDisconnect = null;
  activeDeviceId = null;
  activeStoppers = [];
}

export async function attachLandlinkClient(
  deviceId: string,
  name: string,
): Promise<void> {
  // WHY: an in-flight retry can race with an active connection. If we're
  // already attached to this exact device, leaving the existing notifications
  // alone is safe and avoids a brief "connecting" flicker.
  if (activeDeviceId === deviceId && getState()?.status === "connected") {
    return;
  }
  if (activeDeviceId && activeDeviceId !== deviceId) {
    await detachLandlinkClient(activeDeviceId);
  }

  setConnecting({ deviceId, name });
  activeDeviceId = deviceId;
  seqCounter = 0;

  activeUnsubDisconnect = onLandlinkDisconnect(deviceId, () => {
    cancelAllRetries();
    failAllOutgoingPending();
    void runStoppers().finally(() => {
      clearActive();
      setDisconnected();
    });
  });

  try {
    const stopState = await startNotifications(
      deviceId,
      LANDLINK_SERVICE_UUID,
      LANDLINK_CHARACTERISTIC.STATE,
      (data) => {
        const b = data[0];
        if (b === undefined) return;
        setFsmState(b as FsmStateValue);
      },
    );
    activeStoppers.push(stopState);

    const stopEvt = await startNotifications(
      deviceId,
      LANDLINK_SERVICE_UUID,
      LANDLINK_CHARACTERISTIC.EVT,
      (data) => {
        const frame = decodeFrame(data);
        if (!frame) return;
        const op = frame.opcode as number;
        // Fan out to generic subscribers (channel-list/result, future
        // unowned opcodes) before the client's own handlers — keeps the
        // visible-side-effects ordering deterministic for consumers that
        // also subscribe to store updates.
        broadcastEvt({ opcode: op, seq: frame.seq, payload: frame.payload });
        if (op === Opcode.DEVICE_TELEMETRY) {
          setTelemetry(parseTelemetry(frame.payload));
        } else if (op === Opcode.MESH_SEND_RESULT) {
          let pktId: number | null = null;
          for (const t of decodeTlvs(frame.payload)) {
            if (t.tag === TlvTag.ACK_PKT_ID) {
              pktId = readU32LE(t.value);
              break;
            }
          }
          if (pktId !== null) attachPktId(frame.seq, pktId);
        } else if (op === Opcode.MESH_RECV) {
          const parsed = parseMeshRecv(frame.payload);
          if (!parsed) return;

          // ACKs first — the implicit-broadcast-ACK path (Meshtastic mode
          // hearing its own relayed broadcast) intentionally reports
          // senderNodeId == self_id, so it must not be filtered by the
          // chat-echo guard below.
          if (parsed.kind === "ack") {
            onAck(parsed.ackPktId);
            return;
          }

          // Defense in depth: drop chat frames that purport to come from us.
          const selfNodeId = getState()?.info?.nodeId;
          if (selfNodeId && parsed.senderNodeId === selfNodeId) return;
          const dedupKey = parsed.pktId !== null
            ? `${parsed.senderNodeId}:${parsed.pktId}`
            : `${parsed.senderNodeId}:${parsed.receivedAt}`;
          if (chatSeen(dedupKey)) return;
          const msg: MeshMessage = {
            senderNodeId: parsed.senderNodeId,
            text: parsed.text,
            direction: "incoming",
            receivedAt: parsed.receivedAt,
            channelIndex: parsed.channelIndex,
          };
          if (parsed.pktId !== null) msg.pktId = parsed.pktId;
          appendMessage(msg);
        } else if (op === Opcode.LORA_PEER_FOUND) {
          for (const handler of peerFoundHandlers) {
            try {
              handler({ seq: frame.seq, payload: frame.payload });
            } catch {
              // handlers must not break the EVT stream
            }
          }
        } else if (op === Opcode.RADIO_PROTOCOL_RESULT) {
          const tlvs = decodeTlvs(frame.payload);
          for (const t of tlvs) {
            if (t.tag === TlvTag.PROTOCOL && t.value.byteLength === 1) {
              const v = t.value[0];
              if (v === 0 || v === 1) setProtocol(v);
              break;
            }
          }
        } else if (op === Opcode.ERROR) {
          // Firmware reports a per-command rejection (BAD_ARG, NOT_FOUND,
          // BUSY, UNAUTHED, ...). The payload is the legacy 3-byte tuple
          // [0xF0, 0x01, err_code] used by send_error() — surface it to the
          // console so silent rejections (e.g. a CHANNEL_SET that the
          // firmware refused) are diagnosable without a serial monitor.
          const errCode =
            frame.payload.byteLength >= 3 && frame.payload[0] === 0xf0
              ? (frame.payload[2] ?? null)
              : null;
          console.warn(
            "[landlink] ERROR seq=" + frame.seq.toString() +
              " err=" + (errCode === null ? "?" : "0x" + errCode.toString(16).padStart(2, "0")),
          );
        } else {
          setLastEvtFrame(frame);
        }
      },
    );
    activeStoppers.push(stopEvt);

    try {
      const infoBytes = await readCharacteristic(
        deviceId,
        LANDLINK_SERVICE_UUID,
        LANDLINK_CHARACTERISTIC.INFO,
      );
      const hex = Array.from(infoBytes)
        .map((b) => b.toString(16).padStart(2, "0"))
        .join(" ");
      console.log("[landlink] INFO read", {
        deviceId,
        bytes: infoBytes.byteLength,
        hex,
      });
      if (infoBytes.byteLength > 0) {
        const info = parseLandlinkInfo(infoBytes);
        console.log("[landlink] INFO parsed", info);
        setInfo(info);
        if (info.nodeId) {
          const registered = findDevice(getRegisteredDevices(), deviceId);
          console.log("[landlink] INFO matched registered", {
            registeredNodeId: registered?.nodeId ?? null,
            newNodeId: info.nodeId,
            willUpdate: registered !== null && registered.nodeId !== info.nodeId,
          });
          if (registered && registered.nodeId !== info.nodeId) {
            updateRegisteredDevice(deviceId, { nodeId: info.nodeId });
          }
        }
      } else {
        console.warn("[landlink] INFO read returned 0 bytes");
      }
    } catch (err) {
      // INFO read failure is non-fatal: keep the link alive but record nothing.
      console.warn("[landlink] INFO read failed", err);
    }

    setConnected();
    // Fetch current protocol mode after the link is up. Non-fatal on failure.
    try {
      await sendLandlinkCommand(Opcode.RADIO_GET_PROTOCOL);
    } catch (err) {
      console.warn("[landlink] RADIO_GET_PROTOCOL failed", err);
    }
  } catch (err) {
    await runStoppers();
    clearActive();
    setDisconnected();
    throw err;
  }
}

export async function detachLandlinkClient(deviceId: string): Promise<void> {
  await runStoppers();
  try {
    await disconnectLandlinkDevice(deviceId);
  } catch {
    // ignore: device may already be gone
  }
  clearActive();
  setDisconnected();
}

// Web Bluetooth serializes GATT ops per-device but throws synchronously when
// a second op starts before the first completes ("GATT operation already in
// progress"). When the device first connects we have multiple callers
// queuing writes back-to-back (RADIO_GET_PROTOCOL from attachLandlinkClient,
// CHANNEL_LIST from useSyncDeviceChannels, future channel reads), so we
// serialize at this layer via a per-process chain rather than asking each
// caller to coordinate. A single FIFO is fine because we only ever talk to
// one device at a time (the active session).
let writeChain: Promise<unknown> = Promise.resolve();

export async function sendLandlinkCommand(
  opcode: OpcodeValue,
  tlvs: readonly Tlv[] = [],
  onSeqAssigned?: (seq: number) => void,
): Promise<number> {
  const dev = getState();
  if (dev?.status !== "connected") {
    throw new Error("Landlink device not connected");
  }
  const seq = nextSeq();
  // WHY: firmware may emit MESH_SEND_RESULT via BLE notify before the
  // writeCharacteristic await resolves on a fast link. Letting the caller
  // register state here closes the race — by the time the notify lands, the
  // pending entry already exists.
  onSeqAssigned?.(seq);
  const frame = encodeFrame(opcode, seq, encodeTlvs(tlvs));
  const deviceId = dev.deviceId;
  const run = writeChain.then(() =>
    writeCharacteristic(
      deviceId,
      LANDLINK_SERVICE_UUID,
      LANDLINK_CHARACTERISTIC.CMD,
      frame,
    ),
  );
  // Keep the chain alive even if this write rejects; the next caller still
  // needs a serialization point, just not the failure.
  writeChain = run.catch(() => undefined);
  await run;
  return seq;
}

// Register sendLandlinkCommand with the retry tracker so retransmissions can
// originate without features/ importing api/ (which would be a sideways layer
// import). Done once at module load.
setRetrySender((opcode, retryTlvs) =>
  sendLandlinkCommand(opcode as OpcodeValue, retryTlvs),
);

// Watch for protocol-mode transitions. Switching radios re-tunes the SX1262
// and (in Meshtastic) flips encryption keys, so any in-flight pending chat
// must be abandoned in either direction — its ACK can never round-trip.
let lastSeenProtocol: ProtocolMode | null = null;
subscribeStore(() => {
  const next = getState()?.protocol ?? null;
  if (
    lastSeenProtocol !== null &&
    next !== null &&
    lastSeenProtocol !== next
  ) {
    cancelAllRetries();
    failAllOutgoingPending();
  }
  lastSeenProtocol = next;
});

export async function setLandlinkProtocolMode(mode: ProtocolMode): Promise<void> {
  await sendLandlinkCommand(Opcode.RADIO_SET_PROTOCOL, [
    { tag: TlvTag.PROTOCOL, value: new Uint8Array([mode]) },
  ]);
}

export function appendOutgoingMessage(text: string, channelIndex = 0): void {
  const dev = getState();
  appendMessage({
    senderNodeId: dev?.info?.nodeId ?? "self",
    text,
    direction: "outgoing",
    receivedAt: Date.now(),
    channelIndex,
  });
}

// Landlink-only: append an outgoing chat in "sending" state, tagged with the
// BLE seq used to write the MESH_SEND command. The retry tracker correlates
// this with the firmware's MESH_SEND_RESULT to attach a pkt_id and drive the
// "delivered"/"failed" transitions.
export function appendOutgoingPending(
  text: string,
  bleSeq: number,
  channelIndex = 0,
): void {
  const dev = getState();
  appendMessage({
    senderNodeId: dev?.info?.nodeId ?? "self",
    text,
    direction: "outgoing",
    receivedAt: Date.now(),
    channelIndex,
    bleSeq,
    status: "sending",
    attempts: 1,
  });
}

// Hand the retry tracker the bytes it needs for retransmit/ACK matching.
// Pass { noRetry: true } in Meshtastic mode — that path has no RETRY_PKT_ID
// equivalent so the tracker should only listen for the ACK, never retransmit.
export function trackPendingChat(
  bleSeq: number,
  text: string,
  encodedText: Uint8Array,
  options: TrackPendingOptions = {},
): void {
  trackPending(bleSeq, text, encodedText, options);
}

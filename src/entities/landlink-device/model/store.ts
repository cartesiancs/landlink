import type { BleFrame, FsmStateValue } from "@/shared/protocol";

import {
  attachPktIdToMessage,
  patchMessageByPktId,
  persistMessage,
} from "../api/message-store";

export type LandlinkStatus = "disconnected" | "connecting" | "connected";

export type ParsedInfo = {
  // Canonical numeric self id. Null until the INFO characteristic read lands.
  nodeNum: number | null;
  // BE canonical hex of `nodeNum`. Derived; equals nodeNumToHex(nodeNum).
  nodeId: string | null;
  nodeName: string | null;
  meshId: string | null;
  region: number | null;
};

export type GpsFix = {
  latE7: number;
  lonE7: number;
  altM: number;
  hdopX10: number;
  speedKmhX10: number;
};

export type ChargeState = {
  vbus: boolean;
  charging: boolean;
  full: boolean;
  battPresent: boolean;
};

export type DeviceTelemetry = {
  batteryMv: number;
  batteryPct: number;
  chargeState: ChargeState;
  gps: GpsFix | null;
  receivedAt: number;
};

export type MeshMessageDirection = "incoming" | "outgoing";

export type MeshMessageStatus = "sending" | "delivered" | "failed";

export type MeshMessage = {
  // Stable per-message identifier used to correlate the in-memory entry with
  // its persisted IndexedDB row when status updates (ACK delivery, retries)
  // arrive after the initial write.
  id: string;
  // Canonical numeric sender. For outgoing this is self.
  senderNodeNum: number;
  // BE canonical hex of senderNodeNum, kept for IndexedDB query convenience
  // and feed rendering without recomputing every render.
  senderNodeId: string;
  // Set when this row belongs to a unicast DM. Undefined means broadcast on
  // the channel (regular channel chat). Outgoing DMs set this to the peer;
  // incoming DMs set this to the receiving self id.
  recipientNodeNum?: number;
  text: string;
  direction: MeshMessageDirection;
  receivedAt: number;
  // Meshtastic channel index 0..7. Undefined (legacy) is treated as 0 = Primary
  // so existing Landlink-protocol traffic surfaces on the Primary channel.
  channelIndex?: number;
  pktId?: number;
  bleSeq?: number;
  status?: MeshMessageStatus;
  attempts?: number;
  // True when the originating frame was a Meshtastic PKI-encrypted DM
  // (MeshPacket.pki_encrypted = true). The plaintext is already decrypted
  // by the firmware before reaching us; this flag only drives UI indicators.
  pkiEncrypted?: boolean;
};

export type LandlinkDevice = {
  deviceId: string;
  name: string;
  status: LandlinkStatus;
  info: ParsedInfo | null;
  fsmState: FsmStateValue | null;
  lastEvtFrame: BleFrame | null;
  telemetry: DeviceTelemetry | null;
  messages: readonly MeshMessage[];
};

let state: LandlinkDevice | null = null;
const listeners = new Set<() => void>();

function emit(): void {
  for (const l of listeners) l();
}

function newMessageId(): string {
  if (typeof crypto !== "undefined" && typeof crypto.randomUUID === "function") {
    return crypto.randomUUID();
  }
  // Fallback for environments without crypto.randomUUID (older test runners).
  return `m_${Date.now().toString(36)}_${Math.random().toString(36).slice(2, 10)}`;
}

export function getState(): LandlinkDevice | null {
  return state;
}

export function subscribe(l: () => void): () => void {
  listeners.add(l);
  return () => {
    listeners.delete(l);
  };
}

export function setConnecting(d: { deviceId: string; name: string }): void {
  state = {
    deviceId: d.deviceId,
    name: d.name,
    status: "connecting",
    info: null,
    fsmState: null,
    lastEvtFrame: null,
    telemetry: null,
    messages: [],
  };
  emit();
}

export function setConnected(): void {
  if (!state) return;
  state = { ...state, status: "connected" };
  emit();
}

export function setDisconnected(): void {
  if (!state) return;
  state = null;
  emit();
}

export function setInfo(info: ParsedInfo): void {
  if (!state) return;
  state = { ...state, info };
  emit();
}

export function setFsmState(fsmState: FsmStateValue): void {
  if (!state) return;
  state = { ...state, fsmState };
  emit();
}

export function setLastEvtFrame(frame: BleFrame): void {
  if (!state) return;
  state = { ...state, lastEvtFrame: frame };
  emit();
}

export function setTelemetry(telemetry: DeviceTelemetry): void {
  if (!state) return;
  state = { ...state, telemetry };
  emit();
}

// A RADIO_REGION_RESULT EVT can arrive before the INFO characteristic read
// finishes, so fabricate a blank ParsedInfo when info is still null rather
// than no-op'ing — otherwise the live region update would be lost.
export function setRegion(region: number): void {
  if (!state) return;
  const baseInfo: ParsedInfo = state.info ?? {
    nodeNum: null,
    nodeId: null,
    nodeName: null,
    meshId: null,
    region: null,
  };
  state = { ...state, info: { ...baseInfo, region } };
  emit();
}

export type AppendMessageInput =
  & Omit<MeshMessage, "id">
  & Partial<Pick<MeshMessage, "id">>;

export function appendMessage(input: AppendMessageInput): void {
  if (!state) return;
  const message: MeshMessage = {
    ...input,
    id: input.id ?? newMessageId(),
  };
  state = { ...state, messages: [...state.messages, message] };
  emit();
  // Best-effort durable write. The connected deviceId is the per-device key
  // we use to scope history; if the state has gone missing between emit and
  // here, skip persistence. selfNodeNum is needed so the message-store can
  // compute dmPeerNum for index population.
  const deviceId = state.deviceId;
  const selfNodeNum = state.info?.nodeNum ?? 0;
  void persistMessage(message, deviceId, selfNodeNum);
}

export function attachPktIdToOutgoing(bleSeq: number, pktId: number): void {
  if (!state) return;
  const affectedIds: string[] = [];
  const next = state.messages.map((m) => {
    if (
      m.direction === "outgoing" &&
      m.bleSeq === bleSeq &&
      m.pktId === undefined
    ) {
      affectedIds.push(m.id);
      return { ...m, pktId };
    }
    return m;
  });
  if (affectedIds.length === 0) return;
  state = { ...state, messages: next };
  emit();
  for (const id of affectedIds) {
    void attachPktIdToMessage(id, pktId);
  }
}

export function updateOutgoingByPktId(
  pktId: number,
  patch: Partial<Pick<MeshMessage, "status" | "attempts">>,
): void {
  if (!state) return;
  let changed = false;
  const next = state.messages.map((m) => {
    if (m.direction === "outgoing" && m.pktId === pktId) {
      changed = true;
      return { ...m, ...patch };
    }
    return m;
  });
  if (!changed) return;
  const deviceId = state.deviceId;
  state = { ...state, messages: next };
  emit();
  void patchMessageByPktId(deviceId, pktId, patch);
}

export function failAllOutgoingPending(): void {
  if (!state) return;
  let changed = false;
  const affected: { id: string; pktId: number | undefined }[] = [];
  const next = state.messages.map((m) => {
    if (m.direction === "outgoing" && m.status === "sending") {
      changed = true;
      affected.push({ id: m.id, pktId: m.pktId });
      return { ...m, status: "failed" as const };
    }
    return m;
  });
  if (!changed) return;
  const deviceId = state.deviceId;
  state = { ...state, messages: next };
  emit();
  for (const a of affected) {
    if (a.pktId !== undefined) {
      void patchMessageByPktId(deviceId, a.pktId, { status: "failed" });
    }
  }
}

// Merge persisted history for a channel into the in-memory store, preserving
// any live messages (matched by id) that arrived during hydration. Used by
// the channel-chat page on mount/connect so history survives reloads.
export function replaceChannelMessages(
  channelIndex: number,
  persisted: readonly MeshMessage[],
): void {
  if (!state) return;
  const liveById = new Map<string, MeshMessage>();
  const others: MeshMessage[] = [];
  for (const m of state.messages) {
    if ((m.channelIndex ?? 0) === channelIndex) {
      liveById.set(m.id, m);
    } else {
      others.push(m);
    }
  }
  const merged: MeshMessage[] = [];
  const seen = new Set<string>();
  for (const p of persisted) {
    // Live entries (with updated status/pktId) take precedence over the
    // disk snapshot, which may be stale relative to in-flight ACK updates.
    merged.push(liveById.get(p.id) ?? p);
    seen.add(p.id);
  }
  // Live messages that aren't on disk yet (just appended, persist still in
  // flight) need to stay in the rendered list.
  for (const [id, m] of liveById) {
    if (!seen.has(id)) merged.push(m);
  }
  merged.sort((a, b) => a.receivedAt - b.receivedAt);
  state = { ...state, messages: [...others, ...merged] };
  emit();
}

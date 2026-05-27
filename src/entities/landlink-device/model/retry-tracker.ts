import { MeshKind, Opcode, TlvTag, type Tlv } from "@/shared/protocol";

import {
  attachPktIdToOutgoing,
  updateOutgoingByPktId,
} from "./store";

// Sender-side ACK state machine for outgoing chat.
//
// Each MESH_SEND is tracked here until either
//   (a) an ACK arrives within RETRY_TIMEOUT_MS (-> "delivered"), or
//   (b) the entry exhausts its retry budget (landlink: up to MAX_ATTEMPTS
//       retransmissions with RETRY_PKT_ID; meshtastic: noRetry=true -> the
//       first timeout fails the entry), or
//   (c) the BLE link drops / the user switches protocol modes, in which case
//       every pending entry is force-failed.
//
// The host doesn't know the assigned pkt_id until the firmware emits
// MESH_SEND_RESULT. Entries are therefore indexed by bleSeq first and gain a
// pktId asynchronously via attachPktId().
//
// Meshtastic mode reuses this machinery for the ACK matching half only —
// RETRY_PKT_ID has no Meshtastic equivalent on the wire, so retry must be
// disabled via { noRetry: true }.

const RETRY_TIMEOUT_MS = 10_000;
const MAX_ATTEMPTS = 3;
const MAX_PENDING = 50;
// A retry timer that fires before MESH_SEND_RESULT lands gives the result a
// short grace window so the entry can pick up its pktId and retransmit with
// RETRY_PKT_ID set. Without this, the retry would allocate a fresh pkt_id and
// the receiver would treat it as a new message.
const ATTACH_PKT_ID_GRACE_MS = 2_000;

type SendFn = (opcode: number, tlvs: Tlv[]) => Promise<number>;

type PendingEntry = {
  bleSeq: number;
  pktId: number | null;
  text: string;
  encodedText: Uint8Array;
  attempts: number;
  timerId: ReturnType<typeof setTimeout> | null;
  noRetry: boolean;
};

export type TrackPendingOptions = {
  // When true, the entry will fail at the first timeout instead of issuing a
  // RETRY_PKT_ID retransmit. Used for Meshtastic mode, which has no on-wire
  // retry semantics — duplicate sends would allocate a fresh pkt_id and the
  // receiver would render them as new messages.
  noRetry?: boolean;
};

let sendFn: SendFn | null = null;
const pending = new Map<number, PendingEntry>(); // keyed by bleSeq
// Fallback for the unlikely case where MESH_SEND_RESULT arrives before
// trackPending registers (the primary fix is the pre-seq hook in
// sendLandlinkCommand, but this catches any path that bypasses it).
const orphanPktIds = new Map<number, number>();  // bleSeq -> pktId

export function setRetrySender(fn: SendFn): void {
  sendFn = fn;
}

function clearTimer(entry: PendingEntry): void {
  if (entry.timerId !== null) {
    clearTimeout(entry.timerId);
    entry.timerId = null;
  }
}

function dropOldestIfFull(): void {
  if (pending.size < MAX_PENDING) return;
  const oldestKey = pending.keys().next().value;
  if (oldestKey === undefined) return;
  const entry = pending.get(oldestKey);
  if (entry) {
    clearTimer(entry);
    if (entry.pktId !== null) {
      updateOutgoingByPktId(entry.pktId, { status: "failed", attempts: entry.attempts });
    }
    pending.delete(oldestKey);
  }
}

function scheduleTimer(entry: PendingEntry): void {
  clearTimer(entry);
  entry.timerId = setTimeout(() => {
    void handleTimeout(entry.bleSeq);
  }, RETRY_TIMEOUT_MS);
}

async function performRetry(entry: PendingEntry): Promise<void> {
  if (!sendFn || entry.pktId === null) return;
  const tlvs: Tlv[] = [
    { tag: TlvTag.KIND, value: Uint8Array.of(MeshKind.CHAT_TEXT) },
    { tag: TlvTag.CHAT_TEXT, value: entry.encodedText },
    {
      tag: TlvTag.RETRY_PKT_ID,
      value: u32LE(entry.pktId),
    },
  ];
  try {
    await sendFn(Opcode.MESH_SEND, tlvs);
  } catch {
    // BLE write failed: treat as terminal. The disconnect handler usually
    // fires alongside this and will fail the entry.
    fail(entry);
    return;
  }
  if (entry.pktId !== null) {
    updateOutgoingByPktId(entry.pktId, { attempts: entry.attempts });
  }
  scheduleTimer(entry);
}

function fail(entry: PendingEntry): void {
  clearTimer(entry);
  if (entry.pktId !== null) {
    updateOutgoingByPktId(entry.pktId, {
      status: "failed",
      attempts: entry.attempts,
    });
  }
  pending.delete(entry.bleSeq);
}

async function handleTimeout(bleSeq: number): Promise<void> {
  const entry = pending.get(bleSeq);
  if (!entry) return;
  entry.timerId = null;

  // If the firmware never returned a MESH_SEND_RESULT, retrying without a
  // RETRY_PKT_ID would allocate a brand new pkt_id and the receiver would
  // dedup differently from the original. Give the result a short grace window.
  // (Even in noRetry mode we wait for the pktId — without it the host has no
  // way to ever correlate an inbound ACK, so failing immediately would be
  // strictly worse than waiting one grace cycle.)
  if (entry.pktId === null) {
    entry.timerId = setTimeout(() => {
      void handleTimeout(bleSeq);
    }, ATTACH_PKT_ID_GRACE_MS);
    return;
  }

  if (entry.noRetry || entry.attempts >= MAX_ATTEMPTS) {
    fail(entry);
    return;
  }
  entry.attempts += 1;
  await performRetry(entry);
}

export function trackPending(
  bleSeq: number,
  text: string,
  encodedText: Uint8Array,
  options: TrackPendingOptions = {},
): void {
  dropOldestIfFull();
  const entry: PendingEntry = {
    bleSeq,
    pktId: null,
    text,
    encodedText,
    attempts: 1,
    timerId: null,
    noRetry: options.noRetry ?? false,
  };
  scheduleTimer(entry);
  pending.set(bleSeq, entry);
  // If MESH_SEND_RESULT raced past us, pick up the orphaned pktId now.
  const orphanPktId = orphanPktIds.get(bleSeq);
  if (orphanPktId !== undefined) {
    orphanPktIds.delete(bleSeq);
    entry.pktId = orphanPktId;
    attachPktIdToOutgoing(bleSeq, orphanPktId);
    updateOutgoingByPktId(orphanPktId, { status: "sending", attempts: 1 });
  }
}

export function attachPktId(bleSeq: number, pktId: number): void {
  const entry = pending.get(bleSeq);
  if (!entry) {
    if (orphanPktIds.size >= MAX_PENDING) {
      const oldest = orphanPktIds.keys().next().value;
      if (oldest !== undefined) orphanPktIds.delete(oldest);
    }
    orphanPktIds.set(bleSeq, pktId);
    return;
  }
  if (entry.pktId !== null) return;
  entry.pktId = pktId;
  attachPktIdToOutgoing(bleSeq, pktId);
  updateOutgoingByPktId(pktId, { status: "sending", attempts: entry.attempts });
}

export function onAck(pktId: number): void {
  for (const [bleSeq, entry] of pending) {
    if (entry.pktId === pktId) {
      clearTimer(entry);
      pending.delete(bleSeq);
      updateOutgoingByPktId(pktId, {
        status: "delivered",
        attempts: entry.attempts,
      });
      return;
    }
  }
}

export function cancelAll(): void {
  for (const entry of pending.values()) {
    clearTimer(entry);
    if (entry.pktId !== null) {
      updateOutgoingByPktId(entry.pktId, {
        status: "failed",
        attempts: entry.attempts,
      });
    }
  }
  pending.clear();
}

function u32LE(v: number): Uint8Array {
  const out = new Uint8Array(4);
  out[0] = v & 0xff;
  out[1] = (v >>> 8) & 0xff;
  out[2] = (v >>> 16) & 0xff;
  out[3] = (v >>> 24) & 0xff;
  return out;
}

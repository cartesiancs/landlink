import type { BleFrame, FsmStateValue } from "@/shared/protocol";

export type LandlinkStatus = "disconnected" | "connecting" | "connected";

export type ParsedInfo = {
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

export type MeshMessage = {
  senderNodeId: string;
  text: string;
  direction: MeshMessageDirection;
  receivedAt: number;
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

const MAX_MESSAGES = 50;

let state: LandlinkDevice | null = null;
const listeners = new Set<() => void>();

function emit(): void {
  for (const l of listeners) l();
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

export function appendMessage(message: MeshMessage): void {
  if (!state) return;
  const next = state.messages.length >= MAX_MESSAGES
    ? [...state.messages.slice(1), message]
    : [...state.messages, message];
  state = { ...state, messages: next };
  emit();
}

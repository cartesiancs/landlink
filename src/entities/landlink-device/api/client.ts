import {
  disconnectLandlinkDevice,
  onLandlinkDisconnect,
  readCharacteristic,
  startNotifications,
  writeCharacteristic,
} from "@/shared/api";
import {
  decodeFrame,
  encodeFrame,
  encodeTlvs,
  LANDLINK_CHARACTERISTIC,
  LANDLINK_SERVICE_UUID,
  Opcode,
  type FsmStateValue,
  type OpcodeValue,
  type Tlv,
} from "@/shared/protocol";

import { parseLandlinkInfo } from "../lib/parse-info";
import { parseMeshRecv } from "../lib/parse-mesh-recv";
import { parseTelemetry } from "../lib/parse-telemetry";
import {
  appendMessage,
  getState,
  setConnected,
  setConnecting,
  setDisconnected,
  setFsmState,
  setInfo,
  setLastEvtFrame,
  setTelemetry,
} from "../model/store";

let seqCounter = 0;
function nextSeq(): number {
  seqCounter = (seqCounter + 1) & 0xff;
  return seqCounter;
}

let activeDeviceId: string | null = null;
let activeStoppers: (() => Promise<void>)[] = [];
let activeUnsubDisconnect: (() => void) | null = null;

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
  if (activeDeviceId && activeDeviceId !== deviceId) {
    await detachLandlinkClient(activeDeviceId);
  }

  setConnecting({ deviceId, name });
  activeDeviceId = deviceId;
  seqCounter = 0;

  activeUnsubDisconnect = onLandlinkDisconnect(deviceId, () => {
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
        if (op === Opcode.DEVICE_TELEMETRY) {
          setTelemetry(parseTelemetry(frame.payload));
        } else if (op === Opcode.MESH_RECV) {
          const msg = parseMeshRecv(frame.payload);
          if (msg) appendMessage(msg);
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
      if (infoBytes.byteLength > 0) {
        setInfo(parseLandlinkInfo(infoBytes));
      } else {
        console.warn("[landlink] INFO read returned 0 bytes");
      }
    } catch (err) {
      // INFO read failure is non-fatal: keep the link alive but record nothing.
      console.warn("[landlink] INFO read failed", err);
    }

    setConnected();
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

export async function sendLandlinkCommand(
  opcode: OpcodeValue,
  tlvs: readonly Tlv[] = [],
): Promise<number> {
  const dev = getState();
  if (dev?.status !== "connected") {
    throw new Error("Landlink device not connected");
  }
  const seq = nextSeq();
  const frame = encodeFrame(opcode, seq, encodeTlvs(tlvs));
  await writeCharacteristic(
    dev.deviceId,
    LANDLINK_SERVICE_UUID,
    LANDLINK_CHARACTERISTIC.CMD,
    frame,
  );
  return seq;
}

export function appendOutgoingMessage(text: string): void {
  const dev = getState();
  appendMessage({
    senderNodeId: dev?.info?.nodeId ?? "self",
    text,
    direction: "outgoing",
    receivedAt: Date.now(),
  });
}

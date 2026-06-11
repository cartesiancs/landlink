import {
  appendMessage,
  appendOutgoingMessage,
  failAllOutgoingPending,
  getState,
  setConnected,
  setConnecting,
  setDisconnected,
  setInfo,
} from "@/entities/landlink-device";
import {
  findDevice,
  getRegisteredDevices,
  updateRegisteredDevice,
} from "@/entities/registered-device";
import {
  clearChannels,
  setChannels,
  type Channel as ChannelStoreEntry,
} from "@/entities/meshtastic-channel";
import {
  disconnectLandlinkDevice,
  onLandlinkDisconnect,
  readCharacteristic,
  startNotifications,
  writeCharacteristic,
} from "@/shared/api";
import {
  CHANNEL_ROLE,
  MESHTASTIC_CHARACTERISTIC,
  MESHTASTIC_SERVICE_UUID,
  PORTNUM,
  BROADCAST_ADDR,
  decodeFromRadio,
  encodeToRadio,
  encodeUser,
  type MeshtasticChannel,
  type User as MeshtasticUser,
} from "@/shared/protocol/meshtastic";

let activeDeviceId: string | null = null;
let activeStoppers: (() => Promise<void>)[] = [];
let activeUnsubDisconnect: (() => void) | null = null;
// Channels arrive one at a time via FromRadio; accumulate until
// config_complete_id, then push the full set to the channel store.
let pendingChannels: ChannelStoreEntry[] = [];
// Cached self node id from FromRadio.my_info.my_node_num. Used by sendText so
// we can attribute outgoing messages to a stable id (matches incoming RX
// dedup keying by senderNodeId).
let selfNodeNum = 0;

// Cached self User struct from FromRadio.node_info when ni.num === selfNodeNum.
// We carry the full User on the wire when requesting NodeInfo from a peer
// (NodeInfoModule treats the payload as the requester's identity). Without
// this we have no payload to send and can't trigger a NodeInfo reply.
let selfUser: MeshtasticUser | null = null;

// Default hop limit applied to all outgoing MeshPackets. Meshtastic firmware
// only auto-fills hop_limit when (RX_SRC_USER && want_ack && hop_limit == 0),
// so broadcasts addressed with want_ack=false silently ship with 0 hops and
// never propagate past the local node. We default to 3 to match the
// firmware's getConfiguredOrDefaultHopLimit fallback.
const DEFAULT_HOP_LIMIT = 3;

// External subscribers to NodeInfo events. Exposed so the meshtastic-pki
// entity (same FSD layer) can be fed via a features-layer adapter, without
// landlink/meshtastic-device having to import meshtastic-pki directly.
export type MeshtasticNodeInfoEvent = {
  nodeNum: number; // canonical numeric id
  nodeId: string; // 8-char BE canonical hex, derived from nodeNum
  publicKey?: Uint8Array; // 32 B when present in User.public_key
};
type NodeInfoHandler = (event: MeshtasticNodeInfoEvent) => void;
const nodeInfoHandlers = new Set<NodeInfoHandler>();

export function onMeshtasticNodeInfo(handler: NodeInfoHandler): () => void {
  nodeInfoHandlers.add(handler);
  return () => {
    nodeInfoHandlers.delete(handler);
  };
}

function nodeIdHex(n: number): string {
  // Meshtastic node ids are 32-bit, conventionally displayed as 8-hex (no 0x).
  return (n >>> 0).toString(16).padStart(8, "0");
}

function pskRefToBytes(psk: Uint8Array): Uint8Array {
  // Meshtastic encodes Primary's "default" key as a single byte 0x01. The
  // device-side firmware expands that to the documented 32-byte AES-256
  // key when actually encrypting. For our local representation we mirror
  // the bytes the device sent verbatim; STEP 3 (channel sharing) is where
  // exact key material matters and we'd expand client-side too. For now
  // we keep raw to preserve round-trip fidelity.
  return psk;
}

function toStoreChannel(mc: MeshtasticChannel): ChannelStoreEntry | null {
  if (mc.role === CHANNEL_ROLE.DISABLED) return null;
  const name =
    mc.settings?.name && mc.settings.name.length > 0
      ? mc.settings.name
      : mc.role === CHANNEL_ROLE.PRIMARY
        ? "Primary"
        : `Channel ${mc.index.toString()}`;
  return {
    index: mc.index,
    name,
    psk: pskRefToBytes(mc.settings?.psk ?? new Uint8Array(0)),
    role: mc.role === CHANNEL_ROLE.PRIMARY ? "primary" : "secondary",
    createdAt: 0,
  };
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
  if (activeDeviceId) clearChannels(activeDeviceId);
  activeDeviceId = null;
  activeStoppers = [];
  pendingChannels = [];
  selfNodeNum = 0;
  selfUser = null;
}

export function getSelfUser(): MeshtasticUser | null {
  return selfUser;
}

function bytesToHexPreview(data: Uint8Array, max = 32): string {
  const len = Math.min(data.byteLength, max);
  let out = "";
  for (let i = 0; i < len; i++) {
    out += (data[i] ?? 0).toString(16).padStart(2, "0");
    if (i + 1 < len) out += " ";
  }
  if (data.byteLength > max) out += " ...";
  return out;
}

function dispatchFromRadio(data: Uint8Array): void {
  if (data.byteLength === 0) return;
  let msg;
  try {
    msg = decodeFromRadio(data);
  } catch (err) {
    console.warn("[meshtastic] FromRadio decode failed", {
      bytes: data.byteLength,
      hex: bytesToHexPreview(data),
      err,
    });
    return;
  }
  console.log("[meshtastic] rx", msg.kind, {
    bytes: data.byteLength,
    hex: bytesToHexPreview(data),
  });
  switch (msg.kind) {
    case "my_info":
      selfNodeNum = msg.myInfo.myNodeNum;
      console.log("[meshtastic] my_info", {
        myNodeNum: selfNodeNum,
        hex: nodeIdHex(selfNodeNum),
      });
      if (activeDeviceId !== null) {
        const hex = nodeIdHex(selfNodeNum);
        setInfo({
          nodeNum: selfNodeNum,
          nodeId: hex,
          nodeName: null,
          meshId: null,
          region: null,
        });
        const registered = findDevice(getRegisteredDevices(), activeDeviceId);
        if (registered && registered.nodeNum !== selfNodeNum) {
          updateRegisteredDevice(activeDeviceId, {
            nodeNum: selfNodeNum,
            nodeId: hex,
          });
        }
      }
      return;
    case "channel": {
      const entry = toStoreChannel(msg.channel);
      console.log("[meshtastic] channel", {
        index: msg.channel.index,
        role: msg.channel.role,
        name: msg.channel.settings?.name,
        kept: entry !== null,
      });
      if (entry) pendingChannels.push(entry);
      return;
    }
    case "config_complete_id":
      console.log("[meshtastic] config_complete_id", {
        id: msg.id,
        channels: pendingChannels.length,
      });
      if (activeDeviceId) {
        setChannels(activeDeviceId, pendingChannels);
        pendingChannels = [];
      }
      setConnected();
      return;
    case "packet": {
      const p = msg.packet;
      console.log("[meshtastic] packet", {
        from: nodeIdHex(p.from),
        to: nodeIdHex(p.to),
        channel: p.channel,
        portnum: p.decoded?.portnum,
        payloadBytes: p.decoded?.payload.byteLength,
        rxTime: p.rxTime,
        pkiEncrypted: p.pkiEncrypted,
      });
      // The firmware decrypts PKI DMs on-device using its own X25519 keypair
      // and forwards plaintext over BLE alongside the pki_encrypted flag.
      // Encrypted-but-decoded-absent means we cannot read the packet (firmware
      // key mismatch, pre-NodeInfo, or wrong-channel PSK). Surface a warning
      // so the user can diagnose silent receive failures instead of having
      // the packet vanish without a trace.
      if (!p.decoded) {
        if (p.encrypted) {
          console.warn("[meshtastic] dropped undecodable packet", {
            from: nodeIdHex(p.from),
            to: nodeIdHex(p.to),
            channel: p.channel,
            pktId: p.id,
            pkiEncrypted: p.pkiEncrypted === true,
            encryptedBytes: p.encrypted.byteLength,
          });
        }
        return;
      }
      if (p.decoded.portnum !== PORTNUM.TEXT_MESSAGE_APP) return;
      const text = new TextDecoder().decode(p.decoded.payload);
      const senderHex = nodeIdHex(p.from);
      // Receiver-stamped rx_time is epoch seconds. When 0 (device clock
      // unset) fall back to "now" so the UI shows a real timestamp instead
      // of 1970-01-01.
      const receivedAt =
        p.rxTime > 0 ? p.rxTime * 1000 : Date.now();
      // Defense in depth: drop any TEXT_MESSAGE_APP frame that purports to
      // come from us. Stock Meshtastic firmware does not echo self-originated
      // packets back to the originating BLE phone, but some firmware variants
      // (e.g. Landlink in Meshtastic-compat mode hearing its own relayed
      // broadcast) do. The optimistic append in sendMeshtasticText is the
      // source of truth for our own outgoing messages.
      if (selfNodeNum !== 0 && p.from === selfNodeNum) return;
      const isUnicast = p.to !== BROADCAST_ADDR;
      appendMessage({
        senderNodeNum: p.from,
        senderNodeId: senderHex,
        text,
        direction: "incoming",
        receivedAt,
        channelIndex: p.channel,
        ...(isUnicast ? { recipientNodeNum: p.to } : {}),
        ...(p.pkiEncrypted === true ? { pkiEncrypted: true } : {}),
      });
      return;
    }
    case "node_info": {
      const ni = msg.nodeInfo;
      if (ni.num === 0) return;
      // Cache our own User (id/longName/shortName/hwModel/publicKey) when the
      // firmware emits self NodeInfo. requestMeshtasticNodeInfo needs to ship
      // this verbatim as the NODEINFO_APP payload so the peer's NodeInfoModule
      // treats us as a known requester and replies with its own NodeInfo (and
      // public_key).
      if (selfNodeNum !== 0 && ni.num === selfNodeNum && ni.user) {
        selfUser = ni.user;
      }
      const event: MeshtasticNodeInfoEvent = {
        nodeNum: ni.num >>> 0,
        nodeId: nodeIdHex(ni.num),
      };
      if (ni.user?.publicKey) event.publicKey = ni.user.publicKey;
      for (const handler of nodeInfoHandlers) {
        try {
          handler(event);
        } catch {
          // handlers must not break the FromRadio stream
        }
      }
      return;
    }
    case "unknown":
      // Any FromRadio variant we didn't decode (newer firmware fields,
      // config/module_config, etc.). Safe to ignore.
      return;
  }
}

async function drainFromRadio(deviceId: string): Promise<void> {
  // Read until the device returns an empty buffer (queue drained). This
  // is the standard Meshtastic phone-API contract: each read pops one
  // FromRadio off the device's outbound queue.
  console.log("[meshtastic] drain start", { deviceId });
  let reads = 0;
  for (let i = 0; i < 256; i++) {
    let chunk: Uint8Array;
    try {
      chunk = await readCharacteristic(
        deviceId,
        MESHTASTIC_SERVICE_UUID,
        MESHTASTIC_CHARACTERISTIC.FROM_RADIO,
      );
    } catch (err) {
      console.warn("[meshtastic] drain read failed, stopping", {
        reads,
        err,
      });
      return;
    }
    if (chunk.byteLength === 0) {
      console.log("[meshtastic] drain done (empty)", { reads });
      return;
    }
    reads++;
    dispatchFromRadio(chunk);
  }
  console.warn("[meshtastic] drain hit safety cap", { reads });
}

export async function attachMeshtasticClient(
  deviceId: string,
  name: string,
): Promise<void> {
  console.log("[meshtastic] attach", { deviceId, name });
  if (activeDeviceId === deviceId && getState()?.status === "connected") {
    console.log("[meshtastic] attach: already attached, skipping");
    return;
  }
  if (activeDeviceId && activeDeviceId !== deviceId) {
    await detachMeshtasticClient(activeDeviceId);
  }

  setConnecting({ deviceId, name });
  activeDeviceId = deviceId;
  pendingChannels = [];

  activeUnsubDisconnect = onLandlinkDisconnect(deviceId, () => {
    console.warn("[meshtastic] disconnect callback", { deviceId });
    failAllOutgoingPending();
    void runStoppers().finally(() => {
      clearActive();
      setDisconnected();
    });
  });

  try {
    // Meshtastic's BLE phone API uses TWO notification channels:
    //   • fromRadio — READ only; one FromRadio message per read, empty when
    //     the device's outbound queue is drained.
    //   • fromNum   — NOTIFY; a uint32 counter the device increments after
    //     pushing new data, used as a wake hint so phones can sleep instead
    //     of polling.
    // Subscribing to fromRadio itself throws NotSupportedError (it has no
    // NOTIFY property). We subscribe to fromNum and drain fromRadio on each
    // hint.
    console.log("[meshtastic] subscribe fromNum (wake hint)");
    let draining = false;
    const wake = () => {
      if (draining) return;
      draining = true;
      void drainFromRadio(deviceId).finally(() => {
        draining = false;
      });
    };
    const stopFromNum = await startNotifications(
      deviceId,
      MESHTASTIC_SERVICE_UUID,
      MESHTASTIC_CHARACTERISTIC.FROM_NUM,
      (data) => {
        // The notification payload is the uint32 counter; we don't actually
        // need its value — the change itself is the signal.
        console.log("[meshtastic] fromNum notify", {
          bytes: data.byteLength,
        });
        wake();
      },
    );
    activeStoppers.push(stopFromNum);

    // Kick off the configuration flow. The id is just a tag we'd recognize in
    // FromRadio.config_complete_id; any non-zero value works for one-shot use.
    const wantConfigId = (Math.random() * 0x7fffffff) | 0 || 1;
    const toRadio = encodeToRadio({
      kind: "want_config_id",
      id: wantConfigId,
    });
    console.log("[meshtastic] write want_config_id", {
      id: wantConfigId,
      bytes: toRadio.byteLength,
    });
    await writeCharacteristic(
      deviceId,
      MESHTASTIC_SERVICE_UUID,
      MESHTASTIC_CHARACTERISTIC.TO_RADIO,
      toRadio,
    );

    // Drain the initial config burst (channels, my_info, node_info, then
    // config_complete_id which flips us to "connected" via dispatchFromRadio).
    // The fromNum notification will also fire and trigger another drain via
    // wake(), but the in-flight guard prevents overlap.
    await drainFromRadio(deviceId);
    console.log("[meshtastic] attach complete");
  } catch (err) {
    console.warn("[meshtastic] attach failed", { deviceId, err });
    await runStoppers();
    clearActive();
    setDisconnected();
    throw err;
  }
}

export async function detachMeshtasticClient(deviceId: string): Promise<void> {
  console.log("[meshtastic] detach", { deviceId });
  await runStoppers();
  try {
    await disconnectLandlinkDevice(deviceId);
  } catch {
    // device may already be gone
  }
  clearActive();
  setDisconnected();
}

export type SendMeshtasticTextOptions = {
  dest?: number;
  hopLimit?: number;
};

export async function sendMeshtasticText(
  text: string,
  channelIndex: number,
  options: SendMeshtasticTextOptions = {},
): Promise<void> {
  const dest = options.dest ?? BROADCAST_ADDR;
  const dev = getState();
  if (dev?.status !== "connected") {
    throw new Error("Meshtastic device not connected");
  }
  const trimmed = text.trim();
  if (trimmed.length === 0) throw new Error("Message is empty");
  const payload = new TextEncoder().encode(trimmed);
  if (payload.byteLength > 200) {
    throw new Error("Message exceeds 200 bytes");
  }
  // 32-bit packet id, non-zero. Receivers use this for dedup; PKI encryption
  // (when the firmware decides to apply it) rebuilds its AES-CCM nonce from
  // this exact value.
  const id = ((Math.random() * 0x7fffffff) | 0) || 1;
  const hopLimit = options.hopLimit ?? DEFAULT_HOP_LIMIT;
  // pki_encrypted and public_key (field 16) are intentionally NOT set here.
  // The Meshtastic firmware decides PKI vs PSK on its own based on NodeDB
  // state (recipient's cached public_key + our private_key). The standard
  // phone app leaves these fields untouched so the firmware can pick the
  // right path; setting them client-side would only cause PKI_FAILED on a
  // key mismatch. Our job is to ship hop_limit + Data.source correctly and
  // make sure the recipient has been NodeInfo-bootstrapped beforehand.
  const frame = encodeToRadio({
    kind: "packet",
    packet: {
      to: dest,
      channel: channelIndex,
      id,
      hopLimit,
      decoded: {
        portnum: PORTNUM.TEXT_MESSAGE_APP,
        payload,
        ...(selfNodeNum !== 0 ? { source: selfNodeNum } : {}),
      },
      wantAck: dest !== BROADCAST_ADDR,
    },
  });
  console.log("[meshtastic] sendText", {
    channelIndex,
    dest: nodeIdHex(dest),
    bytes: payload.byteLength,
    pktId: id,
    frameBytes: frame.byteLength,
  });
  try {
    await writeCharacteristic(
      dev.deviceId,
      MESHTASTIC_SERVICE_UUID,
      MESHTASTIC_CHARACTERISTIC.TO_RADIO,
      frame,
    );
  } catch (err) {
    console.warn("[meshtastic] sendText write failed", err);
    throw err;
  }
  // Stock Meshtastic firmware does not echo self-originated TEXT_MESSAGE_APP
  // packets back to the originating BLE phone, so we must append locally for
  // the message to appear in our own feed. dispatchFromRadio drops any
  // echoed copy from firmware variants that do re-emit it. The firmware
  // decides PKI vs PSK on-device; outgoing local echo is plaintext and
  // doesn't carry the pki flag (no auth context to display anyway). When
  // addressing a unicast DM we mirror the destination so the host demuxes
  // the outgoing copy into the DM thread instead of the channel feed.
  const isUnicastOut = dest !== BROADCAST_ADDR;
  appendOutgoingMessage(
    trimmed,
    channelIndex,
    isUnicastOut ? { recipientNodeNum: dest } : {},
  );
}

export function isMeshtasticActive(): boolean {
  return activeDeviceId !== null;
}

// Send a NODEINFO_APP packet to a specific peer to trigger their
// NodeInfoModule into replying with its own NodeInfo (which carries the
// peer's User.public_key). The standard Meshtastic NodeInfoModule does not
// auto-respond on first hear and applies a 12h cooldown on broadcasts, so an
// explicit unicast request is the only reliable way to learn a fresh peer's
// public key before sending a PKI DM.
//
// The payload is our own User struct serialized with encodeUser; NodeInfoModule
// treats it as the requester's identity. want_response=true is the convention
// for asking the receiver to reply.
export async function requestMeshtasticNodeInfo(dest: number): Promise<void> {
  if (dest === BROADCAST_ADDR || dest === 0) {
    throw new Error("NodeInfo request requires a unicast destination");
  }
  const dev = getState();
  if (dev?.status !== "connected") {
    throw new Error("Meshtastic device not connected");
  }
  if (!selfUser) {
    throw new Error("Self NodeInfo not yet learned from firmware");
  }
  const userBytes = encodeUser(selfUser);
  const id = ((Math.random() * 0x7fffffff) | 0) || 1;
  const frame = encodeToRadio({
    kind: "packet",
    packet: {
      to: dest,
      channel: 0,
      id,
      hopLimit: DEFAULT_HOP_LIMIT,
      decoded: {
        portnum: PORTNUM.NODEINFO_APP,
        payload: userBytes,
        wantResponse: true,
        ...(selfNodeNum !== 0 ? { source: selfNodeNum } : {}),
      },
      wantAck: false,
    },
  });
  console.log("[meshtastic] requestNodeInfo", {
    dest: nodeIdHex(dest),
    pktId: id,
    payloadBytes: userBytes.byteLength,
  });
  await writeCharacteristic(
    dev.deviceId,
    MESHTASTIC_SERVICE_UUID,
    MESHTASTIC_CHARACTERISTIC.TO_RADIO,
    frame,
  );
}

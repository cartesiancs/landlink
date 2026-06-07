// Message-level encoders/decoders for the Meshtastic protobuf subset we use.
//
// We only encode the few fields we set explicitly and decode the few we read,
// skipping the rest. Unknown/future fields pass through harmlessly thanks to
// the skipField fallback in readFields.
//
// Reference: meshtastic-protobufs (https://github.com/meshtastic/protobufs)
//   mesh.proto     — Data, MeshPacket, MyNodeInfo, NodeInfo, User
//   channel.proto  — Channel, ChannelSettings
//   mesh.proto     — FromRadio, ToRadio
//
// Field numbers below match the upstream .proto files exactly.

import { PbReader, PbWriter, WIRE_LEN, readFields } from "./protobuf";

// ---------------------------------------------------------------------------
// PortNum
// ---------------------------------------------------------------------------

export const PORTNUM = {
  UNKNOWN_APP: 0,
  TEXT_MESSAGE_APP: 1,
  POSITION_APP: 3,
  NODEINFO_APP: 4,
  ROUTING_APP: 5,
  ADMIN_APP: 6,
} as const;
export type PortNum = (typeof PORTNUM)[keyof typeof PORTNUM];

// ---------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------

export type Data = {
  portnum: number;
  payload: Uint8Array;
  wantResponse?: boolean;
  dest?: number;
  source?: number;
  requestId?: number;
  replyId?: number;
};

export function decodeData(buf: Uint8Array): Data {
  const out: Data = { portnum: 0, payload: new Uint8Array(0) };
  const r = new PbReader(buf);
  readFields(r, (field, _wire, sub) => {
    switch (field) {
      case 1:
        out.portnum = sub.readVarint();
        return true;
      case 2:
        out.payload = sub.readBytes();
        return true;
      case 3:
        out.wantResponse = sub.readBool();
        return true;
      case 4:
        out.dest = sub.readFixed32();
        return true;
      case 5:
        out.source = sub.readFixed32();
        return true;
      case 6:
        out.requestId = sub.readFixed32();
        return true;
      case 7:
        out.replyId = sub.readFixed32();
        return true;
      default:
        return false;
    }
  });
  return out;
}

export function encodeData(d: Data): Uint8Array {
  const w = new PbWriter();
  if (d.portnum !== 0) w.writeUint32(1, d.portnum);
  if (d.payload.byteLength > 0) w.writeBytes(2, d.payload);
  if (d.wantResponse) w.writeBool(3, true);
  if (d.dest !== undefined && d.dest !== 0) w.writeFixed32(4, d.dest);
  if (d.source !== undefined && d.source !== 0) w.writeFixed32(5, d.source);
  if (d.requestId !== undefined && d.requestId !== 0) w.writeFixed32(6, d.requestId);
  if (d.replyId !== undefined && d.replyId !== 0) w.writeFixed32(7, d.replyId);
  return w.finish();
}

// ---------------------------------------------------------------------------
// MeshPacket
// ---------------------------------------------------------------------------

export const BROADCAST_ADDR = 0xffffffff;

export type MeshPacket = {
  from: number;
  to: number;
  channel: number;
  id: number;
  rxTime: number;
  wantAck: boolean;
  hopLimit: number;
  hopStart: number;
  decoded?: Data;        // present when payload_variant = decoded (field 4)
  encrypted?: Uint8Array; // present when payload_variant = encrypted (field 5)
  // PKI metadata (Meshtastic 2.5+). Always present when the firmware
  // forwards a PKI-encrypted DM; absent on legacy or channel-PSK traffic.
  publicKey?: Uint8Array; // sender's X25519 public key hint (field 16)
  pkiEncrypted?: boolean; // field 17
};

export function decodeMeshPacket(buf: Uint8Array): MeshPacket {
  const out: MeshPacket = {
    from: 0,
    to: 0,
    channel: 0,
    id: 0,
    rxTime: 0,
    wantAck: false,
    hopLimit: 0,
    hopStart: 0,
  };
  const r = new PbReader(buf);
  readFields(r, (field, _wire, sub) => {
    switch (field) {
      case 1:
        out.from = sub.readFixed32();
        return true;
      case 2:
        out.to = sub.readFixed32();
        return true;
      case 3:
        out.channel = sub.readVarint();
        return true;
      case 4: {
        out.decoded = decodeData(sub.readBytes());
        return true;
      }
      case 5:
        out.encrypted = sub.readBytes();
        return true;
      case 6:
        out.id = sub.readFixed32();
        return true;
      case 7:
        out.rxTime = sub.readFixed32();
        return true;
      case 9:
        out.hopLimit = sub.readVarint();
        return true;
      case 10:
        out.wantAck = sub.readBool();
        return true;
      case 15:
        out.hopStart = sub.readVarint();
        return true;
      case 16: {
        const key = sub.readBytes();
        // Silently ignore malformed hints — X25519 public keys are 32 B.
        if (key.byteLength === 32) out.publicKey = key;
        return true;
      }
      case 17:
        out.pkiEncrypted = sub.readBool();
        return true;
      default:
        return false;
    }
  });
  return out;
}

export type MeshPacketInit = {
  to: number;
  channel: number;
  id: number;
  wantAck?: boolean;
  hopLimit?: number;
  // payload_variant — exactly one of decoded or encrypted must be set.
  // PKI DMs use the encrypted variant alongside pkiEncrypted=true and the
  // sender's publicKey hint.
  decoded?: Data;
  encrypted?: Uint8Array;
  publicKey?: Uint8Array; // sender's X25519 public key (32 B), pki only
  pkiEncrypted?: boolean;
};

// Encode a MeshPacket for ToRadio. The device fills in `from` itself (it
// knows its own node id) and sets rx_time / via_mqtt on egress.
export function encodeMeshPacket(p: MeshPacketInit): Uint8Array {
  const hasDecoded = p.decoded !== undefined;
  const hasEncrypted = p.encrypted !== undefined;
  if (hasDecoded === hasEncrypted) {
    throw new Error(
      "encodeMeshPacket: exactly one of decoded or encrypted is required",
    );
  }
  const w = new PbWriter();
  if (p.to !== 0) w.writeFixed32(2, p.to);
  if (p.channel !== 0) w.writeUint32(3, p.channel);
  if (hasDecoded) {
    // `decoded` is non-undefined here due to the XOR guard above; the
    // bang is safe and avoids restructuring the type signature.
    w.writeBytes(4, encodeData(p.decoded!));
  } else {
    w.writeBytes(5, p.encrypted!);
  }
  if (p.id !== 0) w.writeFixed32(6, p.id);
  if (p.wantAck === true) w.writeBool(10, true);
  if (p.hopLimit !== undefined && p.hopLimit !== 0) {
    w.writeUint32(9, p.hopLimit);
  }
  if (p.publicKey?.byteLength === 32) {
    w.writeBytes(16, p.publicKey);
  }
  if (p.pkiEncrypted === true) w.writeBool(17, true);
  return w.finish();
}

// ---------------------------------------------------------------------------
// ChannelSettings + Channel
// ---------------------------------------------------------------------------

export const CHANNEL_ROLE = {
  DISABLED: 0,
  PRIMARY: 1,
  SECONDARY: 2,
} as const;
export type ChannelRoleValue = (typeof CHANNEL_ROLE)[keyof typeof CHANNEL_ROLE];

export type ChannelSettings = {
  psk: Uint8Array;
  name: string;
  uplinkEnabled?: boolean;
  downlinkEnabled?: boolean;
};

export function decodeChannelSettings(buf: Uint8Array): ChannelSettings {
  const out: ChannelSettings = { psk: new Uint8Array(0), name: "" };
  const r = new PbReader(buf);
  readFields(r, (field, _wire, sub) => {
    switch (field) {
      case 3:
        out.psk = sub.readBytes();
        return true;
      case 4:
        out.name = sub.readString();
        return true;
      case 6:
        out.uplinkEnabled = sub.readBool();
        return true;
      case 7:
        out.downlinkEnabled = sub.readBool();
        return true;
      default:
        return false;
    }
  });
  return out;
}

export type Channel = {
  index: number;
  settings?: ChannelSettings;
  role: ChannelRoleValue;
};

export function decodeChannel(buf: Uint8Array): Channel {
  const out: Channel = { index: 0, role: CHANNEL_ROLE.DISABLED };
  const r = new PbReader(buf);
  readFields(r, (field, _wire, sub) => {
    switch (field) {
      case 1: {
        // signed int32 → may be negative for unset slots; we only care about 0..7.
        out.index = sub.readVarint() | 0;
        return true;
      }
      case 2:
        out.settings = decodeChannelSettings(sub.readBytes());
        return true;
      case 3: {
        const v = sub.readVarint();
        out.role =
          v === 1
            ? CHANNEL_ROLE.PRIMARY
            : v === 2
              ? CHANNEL_ROLE.SECONDARY
              : CHANNEL_ROLE.DISABLED;
        return true;
      }
      default:
        return false;
    }
  });
  return out;
}

// ---------------------------------------------------------------------------
// User + NodeInfo + MyNodeInfo
// ---------------------------------------------------------------------------

export type User = {
  id: string;
  longName: string;
  shortName: string;
  hwModel: number;
  // X25519 public key broadcast by a node for PKI DMs (Meshtastic 2.5+).
  // 32 bytes when present; absent on legacy 2.4.x nodes.
  publicKey?: Uint8Array;
};

export function decodeUser(buf: Uint8Array): User {
  const out: User = { id: "", longName: "", shortName: "", hwModel: 0 };
  const r = new PbReader(buf);
  readFields(r, (field, _wire, sub) => {
    switch (field) {
      case 1:
        out.id = sub.readString();
        return true;
      case 2:
        out.longName = sub.readString();
        return true;
      case 3:
        out.shortName = sub.readString();
        return true;
      case 5:
        out.hwModel = sub.readVarint();
        return true;
      case 8: {
        const key = sub.readBytes();
        if (key.byteLength === 32) out.publicKey = key;
        return true;
      }
      default:
        return false;
    }
  });
  return out;
}

export type NodeInfo = {
  num: number;
  user?: User;
  lastHeard?: number;
};

export function decodeNodeInfo(buf: Uint8Array): NodeInfo {
  const out: NodeInfo = { num: 0 };
  const r = new PbReader(buf);
  readFields(r, (field, _wire, sub) => {
    switch (field) {
      case 1:
        out.num = sub.readVarint();
        return true;
      case 2:
        out.user = decodeUser(sub.readBytes());
        return true;
      case 4:
        out.lastHeard = sub.readVarint();
        return true;
      default:
        return false;
    }
  });
  return out;
}

export type MyNodeInfo = {
  myNodeNum: number;
};

export function decodeMyNodeInfo(buf: Uint8Array): MyNodeInfo {
  const out: MyNodeInfo = { myNodeNum: 0 };
  const r = new PbReader(buf);
  readFields(r, (field, _wire, sub) => {
    switch (field) {
      case 1:
        out.myNodeNum = sub.readVarint();
        return true;
      default:
        return false;
    }
  });
  return out;
}

// ---------------------------------------------------------------------------
// ToRadio
// ---------------------------------------------------------------------------
// oneof payload_variant {
//   MeshPacket packet = 1;
//   uint32     want_config_id = 3;
//   bool       disconnect = 4;
//   ...
// }

export type ToRadio =
  | { kind: "packet"; packet: MeshPacketInit }
  | { kind: "want_config_id"; id: number }
  | { kind: "disconnect" };

export function encodeToRadio(msg: ToRadio): Uint8Array {
  const w = new PbWriter();
  if (msg.kind === "packet") {
    w.writeBytes(1, encodeMeshPacket(msg.packet));
  } else if (msg.kind === "want_config_id") {
    w.writeUint32(3, msg.id);
  } else {
    w.writeBool(4, true);
  }
  return w.finish();
}

// ---------------------------------------------------------------------------
// FromRadio (oneof payload_variant)
// ---------------------------------------------------------------------------
// 1 uint32   id
// 2 MeshPacket packet
// 3 MyNodeInfo my_info
// 4 NodeInfo node_info
// 5 Config config           (skipped)
// 6 LogRecord log_record    (skipped)
// 7 uint32 config_complete_id
// 8 bool rebooted           (skipped)
// 9 ModuleConfig module_config (skipped)
// 10 Channel channel
// 11 QueueStatus queueStatus (skipped)
// ...

export type FromRadio =
  | { kind: "unknown" }
  | { kind: "packet"; packet: MeshPacket }
  | { kind: "my_info"; myInfo: MyNodeInfo }
  | { kind: "node_info"; nodeInfo: NodeInfo }
  | { kind: "config_complete_id"; id: number }
  | { kind: "channel"; channel: Channel };

export function decodeFromRadio(buf: Uint8Array): FromRadio {
  // FromRadio is a top-level message with an outer `id` plus a oneof. We
  // ignore `id` and report the first oneof variant we recognize. Multiple
  // payload variants in a single message would be malformed; we take the
  // first matched.
  const r = new PbReader(buf);
  let result: FromRadio = { kind: "unknown" };
  readFields(r, (field, wire, sub) => {
    switch (field) {
      case 1:
        // outer id; ignore
        sub.readVarint();
        return true;
      case 2:
        if (wire === WIRE_LEN) {
          result = { kind: "packet", packet: decodeMeshPacket(sub.readBytes()) };
          return true;
        }
        return false;
      case 3:
        if (wire === WIRE_LEN) {
          result = { kind: "my_info", myInfo: decodeMyNodeInfo(sub.readBytes()) };
          return true;
        }
        return false;
      case 4:
        if (wire === WIRE_LEN) {
          result = {
            kind: "node_info",
            nodeInfo: decodeNodeInfo(sub.readBytes()),
          };
          return true;
        }
        return false;
      case 7:
        result = { kind: "config_complete_id", id: sub.readVarint() };
        return true;
      case 10:
        if (wire === WIRE_LEN) {
          result = { kind: "channel", channel: decodeChannel(sub.readBytes()) };
          return true;
        }
        return false;
      default:
        return false;
    }
  });
  return result;
}

export {
  appendOutgoingMessage,
  appendOutgoingPending,
  attachLandlinkClient,
  detachLandlinkClient,
  onLandlinkEvt,
  onLandlinkPeerFound,
  sendLandlinkCommand,
  setLandlinkProtocolMode,
  trackPendingChat,
  type LandlinkEvtFrame,
  type PeerFoundFrame,
} from "./api/client";
export {
  landlinkChannelDelete,
  landlinkChannelList,
  landlinkChannelSet,
  parseChannelResult,
  type DeviceChannel,
  type DeviceChannelResult,
  type DeviceChannelRole,
} from "./api/channel";
export { parseLandlinkInfo } from "./lib/parse-info";
export { useLandlinkDevice } from "./model/use-landlink-device";
// Connected-device store surface used by transport adapters (Landlink TLV
// adapter lives in api/client.ts; the Meshtastic adapter sits in its own
// entity and writes to this store too). Exposed here as the public API so
// the adapter slice doesn't reach into segment internals.
export {
  appendMessage,
  failAllOutgoingPending,
  getState,
  setConnected,
  setConnecting,
  setDisconnected,
  setInfo,
} from "./model/store";
export type {
  ChargeState,
  DeviceTelemetry,
  GpsFix,
  LandlinkDevice,
  LandlinkStatus,
  MeshMessage,
  MeshMessageDirection,
  MeshMessageStatus,
  ParsedInfo,
  ProtocolMode,
} from "./model/store";

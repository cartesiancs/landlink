export {
  appendOutgoingMessage,
  appendOutgoingPending,
  attachLandlinkClient,
  detachLandlinkClient,
  onLandlinkEvt,
  onLandlinkPeerFound,
  sendLandlinkCommand,
  setLandlinkRegion,
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
export { useChannelMessages } from "./model/use-channel-messages";
// Connected-device store surface used by transport adapters (Landlink TLV
// adapter lives in api/client.ts; the Meshtastic adapter sits in its own
// entity and writes to this store too). Exposed here as the public API so
// the adapter slice doesn't reach into segment internals.
export {
  appendMessage,
  failAllOutgoingPending,
  getState,
  replaceChannelMessages,
  setConnected,
  setConnecting,
  setDisconnected,
  setInfo,
} from "./model/store";
export type {
  AppendMessageInput,
  ChargeState,
  DeviceTelemetry,
  GpsFix,
  LandlinkDevice,
  LandlinkStatus,
  MeshMessage,
  MeshMessageDirection,
  MeshMessageStatus,
  ParsedInfo,
} from "./model/store";
// Persistent message history surface (IndexedDB). loadMessages is consumed
// by pages to hydrate channel chat history; clearAllMessages is wired into
// reset-app-data. Mutation helpers (persist/attach/patch) stay private to
// the slice — adapters reach them via the store mutators.
export { clearAllMessages, loadMessages } from "./api/message-store";

import { Hash, Lock } from "lucide-react";
import { useNavigate, useParams } from "react-router-dom";

import { useLandlinkDevice } from "@/entities/landlink-device";
import {
  displayChannelName,
  findChannel,
  useChannels,
} from "@/entities/meshtastic-channel";
import {
  findDevice,
  useRegisteredDevices,
} from "@/entities/registered-device";
import { SendMeshForm } from "@/features/send-mesh-message";
import { ROUTES } from "@/shared/config";
import { hapticTick } from "@/shared/lib";
import { Button } from "@/shared/ui";
import { MeshMessageFeed } from "@/widgets/mesh-message-feed";
import { PageHeader } from "@/widgets/page-header";

export function ChannelChatPage() {
  const params = useParams();
  const navigate = useNavigate();
  const channels = useChannels();
  const device = useLandlinkDevice();
  const registeredDevices = useRegisteredDevices();
  const registered = device
    ? findDevice(registeredDevices, device.deviceId)
    : null;
  const adapter = registered?.protocol ?? "landlink";
  const indexParam = params["index"];
  const parsedIndex =
    indexParam && /^[0-7]$/.test(indexParam) ? Number(indexParam) : null;
  const channel = findChannel(channels, parsedIndex ?? -1);
  const isPrimary = channel?.role === "primary";
  const isConnected = device?.status === "connected";
  // Send/receive on this channel is supported when:
  //   • Meshtastic — every configured channel (Primary + secondaries)
  //   • Landlink — Primary only (no on-wire channel concept)
  const chatSupported =
    channel !== null && isConnected && (adapter === "meshtastic" || isPrimary);

  return (
    <div className="mx-auto flex h-dvh w-full max-w-[430px] flex-col bg-background">
      <PageHeader
        title={
          <span className="flex items-center gap-2">
            {channel ? (
              isPrimary ? (
                <Lock className="size-4 text-muted-foreground" aria-hidden />
              ) : (
                <Hash className="size-4 text-muted-foreground" aria-hidden />
              )
            ) : null}
            {channel ? displayChannelName(channel) : "Channel"}
          </span>
        }
        fallback={ROUTES.channels}
        backLabel="Back to Channels"
      />
      <main className="flex min-h-0 flex-1 flex-col gap-4 px-4 pt-4 pb-4">
        {!channel ? (
          <div className="flex flex-1 flex-col items-center justify-center gap-3 text-center">
            <p className="text-sm text-muted-foreground">Channel not found.</p>
            <Button
              variant="outline"
              onClick={() => {
                hapticTick();
                void navigate(ROUTES.channels, { replace: true });
              }}
            >
              Back to Channels
            </Button>
          </div>
        ) : !isConnected ? (
          <div className="flex flex-1 flex-col items-center justify-center gap-3 text-center">
            <p className="text-sm text-muted-foreground">
              No device connected.
            </p>
            <Button
              variant="outline"
              onClick={() => {
                hapticTick();
                void navigate(ROUTES.connectBluetooth);
              }}
            >
              Connect a device
            </Button>
          </div>
        ) : chatSupported && channel ? (
          <MeshMessageFeed channelIndex={channel.index} />
        ) : (
          <div className="flex flex-1 flex-col items-center justify-center gap-3 px-4 text-center">
            <p className="text-sm text-muted-foreground">
              This channel isn't supported on the connected device.
            </p>
            <p className="max-w-[280px] text-xs text-muted-foreground">
              Landlink firmware only exposes the Primary channel. Connect a
              Meshtastic device to chat on secondary channels.
            </p>
          </div>
        )}
      </main>
      {chatSupported && channel ? (
        <div
          className="bg-background px-4 pt-3 transition-[padding-bottom] duration-250 ease-[cubic-bezier(0.32,0.72,0,1)]"
          style={{
            paddingBottom:
              "calc(max(env(safe-area-inset-bottom), 0.75rem) + var(--keyboard-inset, 0px))",
          }}
        >
          <SendMeshForm channelIndex={channel.index} />
        </div>
      ) : null}
    </div>
  );
}

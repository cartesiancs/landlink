import { AtSign, Lock, LockOpen } from "lucide-react";
import { useNavigate, useParams } from "react-router-dom";

import { useDmMessages } from "@/entities/dm-thread";
import { useLandlinkDevice } from "@/entities/landlink-device";
import { usePublicKey } from "@/entities/meshtastic-pki";
import { SendMeshForm } from "@/features/send-mesh-message";
import { ROUTES } from "@/shared/config";
import { hapticTick, hexToNodeNum, isCanonicalNodeHex } from "@/shared/lib";
import { Button } from "@/shared/ui";
import { MeshMessageFeed } from "@/widgets/mesh-message-feed";
import { PageHeader } from "@/widgets/page-header";

// Direct message thread. URL carries the peer's BE canonical hex node id;
// invalid hex bounces the user back to the channels list rather than rendering
// a broken feed. The wire still rides Primary channel (Meshtastic standard)
// so the underlying SendMeshForm uses channelIndex=0; addressing happens
// through recipientNodeNum.
export function DmChatPage() {
  const params = useParams();
  const navigate = useNavigate();
  const device = useLandlinkDevice();
  const isConnected = device?.status === "connected";

  const hexParam = params["nodeIdHex"];
  const peerNodeIdHex = isCanonicalNodeHex(hexParam) ? hexParam : null;
  const peerNodeNum = peerNodeIdHex ? hexToNodeNum(peerNodeIdHex) : null;
  const messages = useDmMessages(peerNodeNum);
  const publicKey = usePublicKey(peerNodeNum);
  const pkiEncrypted = publicKey !== null;

  return (
    <div className="mx-auto flex h-dvh w-full max-w-[430px] flex-col bg-background">
      <PageHeader
        title={
          <span className="flex items-center gap-2">
            <AtSign className="size-4 text-muted-foreground" aria-hidden />
            <span className="font-mono">{peerNodeIdHex ?? "DM"}</span>
          </span>
        }
        fallback={ROUTES.channels}
        backLabel="Back to Channels"
      >
        {peerNodeNum !== null ? (
          pkiEncrypted ? (
            <Lock
              className="size-3.5 text-emerald-600 dark:text-emerald-400"
              aria-label="End-to-end encrypted"
            />
          ) : (
            <LockOpen
              className="size-3.5 text-amber-600 dark:text-amber-400"
              aria-label="Channel-encrypted only"
            />
          )
        ) : null}
      </PageHeader>
      <main
        className="flex min-h-0 flex-1 flex-col gap-4 px-4 pt-0"
        style={{
          paddingBottom: "var(--keyboard-inset, 0px)",
          transition: "padding-bottom 250ms cubic-bezier(0.32, 0.72, 0, 1)",
        }}
      >
        {peerNodeNum === null ? (
          <div className="flex flex-1 flex-col items-center justify-center gap-3 text-center">
            <p className="text-sm text-muted-foreground">
              Invalid peer node id.
            </p>
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
        ) : (
          <MeshMessageFeed messages={messages} />
        )}
      </main>
      {peerNodeNum !== null ? (
        <div
          className="bg-background px-4 pt-3"
          style={{
            paddingBottom: "max(env(safe-area-inset-bottom), 0.75rem)",
            transform:
              "translate3d(0, calc(-1 * var(--keyboard-inset, 0px)), 0)",
            transition: "transform 250ms cubic-bezier(0.32, 0.72, 0, 1)",
            willChange: "transform",
          }}
        >
          <SendMeshForm
            channelIndex={0}
            recipientNodeNum={peerNodeNum}
            disabled={!isConnected}
            disabledReason="Connect to a device to send messages."
          />
        </div>
      ) : null}
    </div>
  );
}

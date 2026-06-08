import { MessageCircle } from "lucide-react";

import { type LoraPeer, type LoraPeerSource } from "@/entities/lora-peer";
import { formatLastConnected } from "@/entities/registered-device";
import { cn, hapticTick } from "@/shared/lib";
import { Button } from "@/shared/ui";

type NodeListRowProps = {
  peer: LoraPeer;
  onStartDm: (peer: LoraPeer) => void;
};

const SOURCE_DOT: Record<LoraPeerSource, string> = {
  beacon: "bg-green-500",
  chat: "bg-blue-500",
  history: "bg-muted-foreground/40",
};

export function NodeListRow({ peer, onStartDm }: NodeListRowProps) {
  return (
    <li className="flex items-center justify-between gap-3 rounded-md border border-border px-3 py-2">
      <div className="flex min-w-0 items-center gap-2">
        <span
          aria-hidden="true"
          className={cn(
            "size-2 shrink-0 rounded-full",
            SOURCE_DOT[peer.source],
          )}
        />
        <p className="truncate font-mono text-sm">{peer.nodeId}</p>
      </div>
      <div className="flex shrink-0 items-center gap-2">
        <span className="text-xs text-muted-foreground">
          {formatLastConnected(peer.lastSeenAt)}
        </span>
        <Button
          variant="ghost"
          size="icon"
          aria-label={`Send direct message to ${peer.nodeId}`}
          onClick={() => {
            hapticTick();
            onStartDm(peer);
          }}
        >
          <MessageCircle aria-hidden="true" />
        </Button>
      </div>
    </li>
  );
}

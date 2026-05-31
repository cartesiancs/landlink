import { type LoraPeer } from "@/entities/lora-peer";
import { formatLastConnected } from "@/entities/registered-device";

type NodeListRowProps = {
  peer: LoraPeer;
};

export function NodeListRow({ peer }: NodeListRowProps) {
  return (
    <li className="flex items-center justify-between gap-3 rounded-md border border-border px-3 py-2">
      <p className="truncate font-mono text-sm">{peer.nodeId}</p>
      <p className="shrink-0 text-xs text-muted-foreground">
        {formatLastConnected(peer.lastSeenAt)}
      </p>
    </li>
  );
}

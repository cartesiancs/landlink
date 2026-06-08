import { AtSign, ChevronRight } from "lucide-react";
import { useNavigate } from "react-router-dom";

import { type DmThread } from "@/entities/dm-thread";
import { ROUTES } from "@/shared/config";
import { cn, hapticTick } from "@/shared/lib";

type DmThreadRowProps = {
  thread: DmThread;
};

export function DmThreadRow({ thread }: DmThreadRowProps) {
  const navigate = useNavigate();

  const handleClick = (): void => {
    hapticTick();
    const path = ROUTES.dmChat.replace(":nodeIdHex", thread.peerNodeIdHex);
    void navigate(path, { viewTransition: true });
  };

  return (
    <li
      className={cn(
        "flex items-center gap-3 rounded-md border border-border px-3 py-3 cursor-pointer hover:bg-muted/40",
      )}
      data-kind="dm"
      data-peer={thread.peerNodeIdHex}
      onClick={handleClick}
    >
      <span
        className="flex size-8 shrink-0 items-center justify-center rounded-md bg-muted text-muted-foreground"
        aria-hidden="true"
      >
        <AtSign className="size-4" />
      </span>
      <div className="min-w-0 flex-1">
        <p className="truncate font-mono text-sm font-medium">
          {thread.peerNodeIdHex}
        </p>
        {thread.lastTextPreview ? (
          <p className="mt-0.5 truncate text-xs text-muted-foreground">
            {thread.lastTextPreview}
          </p>
        ) : null}
      </div>
      <ChevronRight
        className="size-4 shrink-0 text-muted-foreground"
        aria-hidden="true"
      />
    </li>
  );
}

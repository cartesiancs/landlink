import { ChevronRight, Hash, Lock, MoreVertical, Trash2 } from "lucide-react";
import { useNavigate } from "react-router-dom";

import { type Channel } from "@/entities/meshtastic-channel";
import { ROUTES } from "@/shared/config";
import { cn, hapticTick } from "@/shared/lib";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/shared/ui";

type ChannelRowProps = {
  channel: Channel;
  onRequestDelete: (channel: Channel) => void;
};

export function ChannelRow({ channel, onRequestDelete }: ChannelRowProps) {
  const navigate = useNavigate();
  const isPrimary = channel.role === "primary";

  const handleClick = () => {
    hapticTick();
    const path = ROUTES.channelChat.replace(
      ":index",
      channel.index.toString(),
    );
    void navigate(path, { viewTransition: true });
  };

  return (
    <li
      className={cn(
        "flex items-center gap-3 rounded-md border border-border px-3 py-3 cursor-pointer hover:bg-muted/40",
      )}
      data-role={channel.role}
      data-index={channel.index}
      onClick={handleClick}
    >
      <span
        className="flex size-8 shrink-0 items-center justify-center rounded-md bg-muted text-muted-foreground"
        aria-hidden="true"
      >
        {isPrimary ? <Lock className="size-4" /> : <Hash className="size-4" />}
      </span>
      <div className="min-w-0 flex-1">
        <div className="flex items-center gap-2">
          <p className="truncate text-sm font-medium">{channel.name}</p>
          {isPrimary ? (
            <span className="shrink-0 rounded-sm border border-border px-1.5 py-[1px] text-[10px] uppercase tracking-wide text-muted-foreground">
              Default
            </span>
          ) : null}
        </div>
        <p className="mt-0.5 truncate text-xs text-muted-foreground">
          Channel #{channel.index}
        </p>
      </div>
      {isPrimary ? (
        <ChevronRight
          className="size-4 shrink-0 text-muted-foreground"
          aria-hidden="true"
        />
      ) : (
        <DropdownMenu>
          <DropdownMenuTrigger asChild>
            <button
              type="button"
              aria-label="Channel options"
              className="flex size-9 shrink-0 items-center justify-center rounded-md text-muted-foreground hover:bg-muted hover:text-foreground"
              onClick={(e) => {
                e.stopPropagation();
                hapticTick();
              }}
            >
              <MoreVertical className="size-5" aria-hidden="true" />
            </button>
          </DropdownMenuTrigger>
          <DropdownMenuContent>
            <DropdownMenuItem
              variant="destructive"
              onSelect={() => {
                hapticTick();
                onRequestDelete(channel);
              }}
            >
              <Trash2 aria-hidden="true" />
              Delete channel
            </DropdownMenuItem>
          </DropdownMenuContent>
        </DropdownMenu>
      )}
    </li>
  );
}

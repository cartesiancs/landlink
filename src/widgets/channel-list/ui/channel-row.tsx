import { ChevronRight, Hash, MoreVertical, Share2, Trash2 } from "lucide-react";
import { useNavigate } from "react-router-dom";

import {
  displayChannelName,
  type Channel,
} from "@/entities/meshtastic-channel";
import { ROUTES } from "@/shared/config";
import { cn, hapticTick } from "@/shared/lib";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuSeparator,
  DropdownMenuTrigger,
} from "@/shared/ui";

type ChannelRowProps = {
  channel: Channel;
  // When false, the row never shows the delete affordance — used in
  // Meshtastic mode where channels are device-managed.
  deletable?: boolean;
  onRequestDelete: (channel: Channel) => void;
  onRequestShare: (channel: Channel) => void;
};

export function ChannelRow({
  channel,
  deletable = true,
  onRequestDelete,
  onRequestShare,
}: ChannelRowProps) {
  const navigate = useNavigate();
  const isPrimary = channel.role === "primary";
  // Share is reachable on every channel (including Primary). The drawer
  // itself surfaces a "key not cached, reconnect" affordance when the
  // firmware hasn't echoed the PSK yet, so we never hide the menu based
  // on PSK presence.
  const showDelete = deletable && !isPrimary;

  const handleClick = () => {
    hapticTick();
    const path = ROUTES.channelChat.replace(":index", channel.index.toString());
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
        <Hash className="size-4" />
      </span>
      <div className="min-w-0 flex-1">
        <div className="flex items-center gap-2">
          <p className="truncate text-sm font-medium">
            {displayChannelName(channel)}
          </p>
        </div>
      </div>
      {isPrimary ? (
        <ChevronRight
          className="size-4 shrink-0 text-muted-foreground"
          aria-hidden="true"
        />
      ) : (
        // WHY the wrapper span: Radix portals retain React's event
        // propagation through the JSX tree, so a click on a
        // DropdownMenuItem (rendered in a portal) bubbles back up to the
        // <li onClick={handleClick}> and would navigate to the chat page.
        // Stopping propagation at the wrapper catches both the trigger
        // click and item clicks before they reach the row.
        <span
          onClick={(e) => {
            e.stopPropagation();
          }}
        >
          <DropdownMenu>
            <DropdownMenuTrigger asChild>
              <button
                type="button"
                aria-label="Channel options"
                className="flex size-9 shrink-0 items-center justify-center rounded-md text-muted-foreground hover:bg-muted hover:text-foreground"
                onClick={() => {
                  hapticTick();
                }}
              >
                <MoreVertical className="size-5" aria-hidden="true" />
              </button>
            </DropdownMenuTrigger>
            <DropdownMenuContent>
              <DropdownMenuItem
                onSelect={() => {
                  hapticTick();
                  onRequestShare(channel);
                }}
              >
                <Share2 aria-hidden="true" />
                Share for Meshtastic
              </DropdownMenuItem>
              {showDelete ? <DropdownMenuSeparator /> : null}
              {showDelete ? (
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
              ) : null}
            </DropdownMenuContent>
          </DropdownMenu>
        </span>
      )}
    </li>
  );
}

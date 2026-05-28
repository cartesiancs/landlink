import { Check, CheckCheck } from "lucide-react";
import { useEffect, useRef } from "react";

import {
  useLandlinkDevice,
  type MeshMessage,
} from "@/entities/landlink-device";
import { cn } from "@/shared/lib";

type GroupedMessage = {
  message: MeshMessage;
  isFirstInGroup: boolean;
  isLastInGroup: boolean;
};

function groupKey(m: MeshMessage): string {
  return m.direction === "outgoing" ? "__me__" : m.senderNodeId;
}

function groupMessages(messages: readonly MeshMessage[]): GroupedMessage[] {
  return messages.map((m, i) => {
    const prev = messages[i - 1];
    const next = messages[i + 1];
    const key = groupKey(m);
    const isFirstInGroup = !prev || groupKey(prev) !== key;
    const isLastInGroup = !next || groupKey(next) !== key;
    return { message: m, isFirstInGroup, isLastInGroup };
  });
}

function DeliveryIndicator({ message }: { message: MeshMessage }) {
  if (message.direction !== "outgoing" || !message.status) return null;
  // Sent and failed look identical so users never see a failure state.
  const delivered = message.status === "delivered";
  const Icon = delivered ? CheckCheck : Check;
  const label = delivered ? "Delivered" : "Sent";
  return (
    <span
      className={cn(
        "mt-0.5 flex items-center px-2",
        delivered ? "text-sky-500" : "text-muted-foreground",
      )}
      aria-label={label}
      title={label}
    >
      <Icon className="size-3" aria-hidden="true" />
    </span>
  );
}

function MessageRow({
  message,
  isFirstInGroup,
  isLastInGroup,
}: GroupedMessage) {
  const outgoing = message.direction === "outgoing";
  return (
    <li
      className={cn(
        "flex flex-col",
        outgoing ? "items-end" : "items-start",
        isFirstInGroup ? "mt-3 first:mt-0" : "mt-0.5",
      )}
    >
      {isFirstInGroup && !outgoing ? (
        <span className="mb-1 px-3 font-mono text-[10px] text-muted-foreground">
          {message.senderNodeId}
        </span>
      ) : null}
      <div
        className={cn(
          "max-w-[80%] rounded-2xl px-3.5 py-2 text-sm wrap-break-word",
          outgoing
            ? "bg-primary text-primary-foreground"
            : "bg-muted text-foreground",
          outgoing && !isFirstInGroup && "rounded-tr-md",
          outgoing && !isLastInGroup && "rounded-br-md",
          !outgoing && !isFirstInGroup && "rounded-tl-md",
          !outgoing && !isLastInGroup && "rounded-bl-md",
        )}
      >
        {message.text}
      </div>
      {outgoing && isLastInGroup ? <DeliveryIndicator message={message} /> : null}
    </li>
  );
}

type MeshMessageFeedProps = {
  // Filter messages to a specific Meshtastic channel index. Undefined channels
  // on legacy MeshMessage entries are treated as Primary (0).
  channelIndex?: number;
};

export function MeshMessageFeed({ channelIndex = 0 }: MeshMessageFeedProps = {}) {
  const device = useLandlinkDevice();
  const allMessages = device?.messages ?? [];
  const messages = allMessages.filter(
    (m) => (m.channelIndex ?? 0) === channelIndex,
  );
  const scrollRef = useRef<HTMLDivElement | null>(null);
  const lastCount = useRef(0);

  useEffect(() => {
    if (messages.length > lastCount.current && scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
    lastCount.current = messages.length;
  }, [messages.length]);

  const grouped = groupMessages(messages);

  return (
    <section className="flex min-h-0 flex-1 flex-col gap-2">
      {messages.length === 0 ? (
        <p className="rounded-md border border-dashed border-border px-3 py-4 text-center text-sm text-muted-foreground">
          No messages yet.
        </p>
      ) : (
        <div
          ref={scrollRef}
          className="flex min-h-0 flex-1 flex-col-reverse overflow-y-auto"
        >
          <ul className="flex flex-col">
            {grouped.map((g, i) => (
              <MessageRow
                key={`${g.message.senderNodeId}-${g.message.receivedAt}-${i}`}
                message={g.message}
                isFirstInGroup={g.isFirstInGroup}
                isLastInGroup={g.isLastInGroup}
              />
            ))}
          </ul>
        </div>
      )}
    </section>
  );
}

import { useEffect, useRef, useState } from "react";

import {
  useLandlinkDevice,
  type MeshMessage,
} from "@/entities/landlink-device";
import { cn } from "@/shared/lib";

function formatRelativeTime(deltaMs: number): string {
  if (deltaMs < 5_000) return "just now";
  if (deltaMs < 60_000) return `${Math.floor(deltaMs / 1000)}s ago`;
  if (deltaMs < 3_600_000) return `${Math.floor(deltaMs / 60_000)}m ago`;
  return `${Math.floor(deltaMs / 3_600_000)}h ago`;
}

function MessageRow({ message, now }: { message: MeshMessage; now: number }) {
  const outgoing = message.direction === "outgoing";
  return (
    <li className={cn("flex", outgoing ? "justify-end" : "justify-start")}>
      <div
        className={cn(
          "flex max-w-[80%] flex-col gap-0.5 rounded-md border px-3 py-2",
          outgoing
            ? "border-primary/20 bg-primary/10"
            : "border-border bg-card",
        )}
      >
        <div className="flex items-center justify-between gap-2 text-[10px] uppercase tracking-wide text-muted-foreground">
          <span className="font-mono">
            {outgoing ? "me" : message.senderNodeId}
          </span>
          <span>{formatRelativeTime(now - message.receivedAt)}</span>
        </div>
        <p className="text-sm wrap-break-word">{message.text}</p>
      </div>
    </li>
  );
}

export function MeshMessageFeed() {
  const device = useLandlinkDevice();
  const messages = device?.messages ?? [];
  const scrollRef = useRef<HTMLDivElement | null>(null);
  const lastCount = useRef(0);
  const [now, setNow] = useState(() => Date.now());

  useEffect(() => {
    const id = window.setInterval(() => {
      setNow(Date.now());
    }, 15_000);
    return () => {
      window.clearInterval(id);
    };
  }, []);

  useEffect(() => {
    if (messages.length > lastCount.current && scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
    lastCount.current = messages.length;
  }, [messages.length]);

  return (
    <section className="flex flex-col gap-2">
      <h2 className="text-sm font-semibold tracking-tight">Chat</h2>
      {messages.length === 0 ? (
        <p className="rounded-md border border-dashed border-border px-3 py-4 text-center text-sm text-muted-foreground">
          No messages yet.
        </p>
      ) : (
        <div
          ref={scrollRef}
          className="flex max-h-72 flex-col-reverse gap-2 overflow-y-auto"
        >
          <ul className="flex flex-col gap-2">
            {messages.map((m, i) => (
              <MessageRow
                key={`${m.senderNodeId}-${m.receivedAt}-${i}`}
                message={m}
                now={now}
              />
            ))}
          </ul>
        </div>
      )}
    </section>
  );
}

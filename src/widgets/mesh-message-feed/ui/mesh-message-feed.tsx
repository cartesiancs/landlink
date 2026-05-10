import { useEffect, useRef, useState } from "react";

import {
  useLandlinkDevice,
  type IncomingMeshMessage,
} from "@/entities/landlink-device";

function formatRelativeTime(deltaMs: number): string {
  if (deltaMs < 5_000) return "just now";
  if (deltaMs < 60_000) return `${Math.floor(deltaMs / 1000)}s ago`;
  if (deltaMs < 3_600_000) return `${Math.floor(deltaMs / 60_000)}m ago`;
  return `${Math.floor(deltaMs / 3_600_000)}h ago`;
}

function MessageRow({
  message,
  now,
}: {
  message: IncomingMeshMessage;
  now: number;
}) {
  return (
    <li className="flex flex-col gap-0.5 rounded-md border border-border bg-card px-3 py-2">
      <div className="flex items-center justify-between text-[10px] uppercase tracking-wide text-muted-foreground">
        <span className="font-mono">{message.senderNodeId}</span>
        <span>{formatRelativeTime(now - message.receivedAt)}</span>
      </div>
      <p className="text-sm wrap-break-word">{message.text}</p>
    </li>
  );
}

export function MeshMessageFeed() {
  const device = useLandlinkDevice();
  const messages = device?.incomingMessages ?? [];
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
      <h2 className="text-sm font-semibold tracking-tight">Incoming</h2>
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

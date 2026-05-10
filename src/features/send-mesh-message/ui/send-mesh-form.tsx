import { useState, type FormEvent } from "react";
import { Send } from "lucide-react";

import { hapticTick } from "@/shared/lib";
import { Button } from "@/shared/ui";

import { useSendMeshMessage } from "../model/use-send-mesh-message";

export function SendMeshForm() {
  const { send, status, error, maxBytes } = useSendMeshMessage();
  const [text, setText] = useState("");

  const byteLength = new TextEncoder().encode(text).byteLength;
  const tooLong = byteLength > maxBytes;
  const sending = status === "sending";

  async function handleSubmit(e: FormEvent<HTMLFormElement>): Promise<void> {
    e.preventDefault();
    if (sending || tooLong || text.trim().length === 0) return;
    hapticTick();
    const ok = await send(text);
    if (ok) setText("");
  }

  return (
    <form className="flex flex-col gap-2" onSubmit={(e) => void handleSubmit(e)}>
      <div className="flex items-end gap-2">
        <textarea
          aria-label="Mesh message"
          value={text}
          onChange={(e) => {
            setText(e.target.value);
          }}
          placeholder="Broadcast to mesh peers"
          rows={2}
          className="flex-1 resize-none rounded-md border border-border bg-background px-3 py-2 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
          disabled={sending}
        />
        <Button
          type="submit"
          size="icon-lg"
          aria-label="Send"
          disabled={sending || tooLong || text.trim().length === 0}
        >
          <Send className="size-4" aria-hidden="true" />
        </Button>
      </div>
      <div className="flex items-center justify-between text-xs text-muted-foreground">
        <span className={tooLong ? "text-destructive" : undefined}>
          {byteLength}/{maxBytes} B
        </span>
        {status === "error" && error ? (
          <span className="text-destructive">{error}</span>
        ) : status === "sent" ? (
          <span className="text-emerald-600">Sent</span>
        ) : null}
      </div>
    </form>
  );
}

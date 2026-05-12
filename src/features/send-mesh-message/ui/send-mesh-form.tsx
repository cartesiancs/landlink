import { useState, type FormEvent } from "react";
import { Send } from "lucide-react";

import { cn, hapticTick } from "@/shared/lib";
import { Button, toast } from "@/shared/ui";

import { useSendMeshMessage } from "../model/use-send-mesh-message";

export function SendMeshForm() {
  const { send, status, maxBytes } = useSendMeshMessage();
  const [text, setText] = useState("");

  const byteLength = new TextEncoder().encode(text).byteLength;
  const tooLong = byteLength > maxBytes;
  const sending = status === "sending";

  async function handleSubmit(e: FormEvent<HTMLFormElement>): Promise<void> {
    e.preventDefault();
    if (sending || text.trim().length === 0) return;
    if (tooLong) {
      toast.error(`Messages over ${maxBytes.toString()} bytes are not allowed`);
      return;
    }
    hapticTick();
    const ok = await send(text);
    if (ok) setText("");
  }

  return (
    <form
      className="flex flex-col gap-1"
      onSubmit={(e) => void handleSubmit(e)}
    >
      <div className="flex items-end gap-2">
        <textarea
          aria-label="Mesh message"
          value={text}
          onChange={(e) => {
            setText(e.target.value);
          }}
          placeholder="Message"
          rows={1}
          className={cn(
            "max-h-32 min-h-9 flex-1 resize-none rounded-2xl border border-border bg-muted px-4 py-2 text-base outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50",
            tooLong &&
              "border-destructive focus-visible:border-destructive focus-visible:ring-destructive/50",
          )}
          disabled={sending}
        />
        <Button
          type="submit"
          size="icon-lg"
          aria-label="Send"
          disabled={sending || text.trim().length === 0}
          className="rounded-full h-10.5 w-10.5"
        >
          <Send className="size-4" aria-hidden="true" />
        </Button>
      </div>
    </form>
  );
}

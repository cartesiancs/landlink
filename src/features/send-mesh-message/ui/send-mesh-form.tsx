import { usePostHog } from "@posthog/react";
import { useRef, useState, type FormEvent } from "react";
import { Loader2, Send } from "lucide-react";

import { cn, hapticTick } from "@/shared/lib";
import {
  Button,
  Dialog,
  DialogContent,
  DialogDescription,
  DialogFooter,
  DialogHeader,
  DialogTitle,
  toast,
} from "@/shared/ui";

import {
  SEND_MESH_ERROR_KEY_TIMEOUT,
  SEND_MESH_ERROR_KEY_UNKNOWN,
  useSendMeshMessage,
} from "../model/use-send-mesh-message";

function fallbackPrompt(
  adapter: "meshtastic" | "landlink",
  reason: "timeout" | "unknown",
): string {
  if (adapter === "landlink") {
    return "Peer's public key is not cached yet. Their next periodic NodeInfo broadcast (about every 15 minutes) will enable PKI. Send with channel PSK fallback now?";
  }
  return reason === "timeout"
    ? "Public key not received from this node. Send with channel PSK fallback instead?"
    : "Public key unknown for this peer. Send with channel PSK fallback instead?";
}

type SendMeshFormProps = {
  // Channel to address. Default 0 = Primary. On Landlink devices only
  // Primary works (the hook errors on other indices); on Meshtastic the
  // index routes via MeshPacket.channel.
  channelIndex?: number;
  // When set, every send addresses this peer as a unicast. The DM chat page
  // passes this to keep the composer scoped to the open thread; the channel
  // chat page leaves it undefined for normal channel broadcast.
  recipientNodeNum?: number;
  // When true the textarea and submit button are both disabled, used by the
  // channel chat page to keep the form mounted (for layout stability and
  // history viewing) while no device is connected to deliver writes.
  disabled?: boolean;
  // Human-readable explanation surfaced via toast when the user taps the
  // form while it is disabled. Without it, taps on the disabled controls
  // are silently swallowed and the affordance feels broken.
  disabledReason?: string;
};

export function SendMeshForm({
  channelIndex = 0,
  recipientNodeNum,
  disabled = false,
  disabledReason,
}: SendMeshFormProps = {}) {
  const { send, status, error, maxBytes, adapter } =
    useSendMeshMessage(channelIndex);
  const [text, setText] = useState("");
  // Pending fallback dialog state. When the send hook reports an unknown or
  // timed-out peer public key we capture the message body the user tried to
  // send and surface a shadcn Dialog asking whether to fall back to channel
  // PSK. Resolving the dialog (confirm or cancel) clears this state.
  const [fallback, setFallback] = useState<{
    body: string;
    reason: "timeout" | "unknown";
  } | null>(null);
  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const posthog = usePostHog();

  const byteLength = new TextEncoder().encode(text).byteLength;
  const tooLong = byteLength > maxBytes;
  const sending = status === "sending";
  const requestingKey = status === "requesting-key";
  const busy = sending || requestingKey;

  async function attemptSend(
    body: string,
    skipPkiBootstrap: boolean,
  ): Promise<boolean> {
    return send(body, {
      ...(recipientNodeNum !== undefined ? { recipientNodeNum } : {}),
      ...(skipPkiBootstrap ? { skipPkiBootstrap: true } : {}),
    });
  }

  function onSuccess(sentBody: string): void {
    posthog.capture("mesh_message_sent", {
      byte_length: new TextEncoder().encode(sentBody).byteLength,
      unicast: recipientNodeNum !== undefined,
    });
    setText((cur) => (cur === sentBody ? "" : cur));
  }

  async function handleSubmit(e: FormEvent<HTMLFormElement>): Promise<void> {
    e.preventDefault();
    if (disabled || busy || text.trim().length === 0) return;
    if (tooLong) {
      toast.error(`Messages over ${maxBytes.toString()} bytes are not allowed`);
      return;
    }
    hapticTick();
    const sent = text;
    // WHY: on iOS Safari, programmatic focus only works inside a user gesture.
    // Refocus synchronously before awaiting the BLE write so the soft keyboard
    // stays open even if a parent rerender or button tap stole focus.
    textareaRef.current?.focus();
    const ok = await attemptSend(sent, false);
    if (ok) {
      onSuccess(sent);
    } else if (
      error === SEND_MESH_ERROR_KEY_TIMEOUT ||
      error === SEND_MESH_ERROR_KEY_UNKNOWN
    ) {
      setFallback({
        body: sent,
        reason: error === SEND_MESH_ERROR_KEY_TIMEOUT ? "timeout" : "unknown",
      });
    }
    textareaRef.current?.focus();
  }

  async function confirmFallback(): Promise<void> {
    if (!fallback) return;
    const body = fallback.body;
    setFallback(null);
    const ok = await attemptSend(body, true);
    if (ok) onSuccess(body);
  }

  const showDisabledReason = (): void => {
    if (disabled && disabledReason) {
      toast.info(disabledReason);
    }
  };

  return (
    <>
    <form
      className="flex flex-col gap-1"
      onSubmit={(e) => void handleSubmit(e)}
      // WHY: native `disabled` on <button>/<textarea> swallows click events, so
      // taps on the inactive form never reach a handler. The pointer-events:none
      // wrapper below routes the click up to the form, where we surface the
      // reason as a toast instead of leaving the interaction silent.
      onClick={showDisabledReason}
    >
      <div
        className={cn(
          "flex items-end gap-2",
          disabled && disabledReason && "pointer-events-none",
        )}
      >
        <textarea
          ref={textareaRef}
          aria-label="Mesh message"
          value={text}
          onChange={(e) => {
            setText(e.target.value);
          }}
          onKeyDown={(e) => {
            // WHY: nativeEvent.isComposing guards IME composition (e.g. Korean)
            // so confirming a candidate with Enter does not submit the form.
            if (
              e.key === "Enter" &&
              !e.shiftKey &&
              !e.nativeEvent.isComposing
            ) {
              e.preventDefault();
              e.currentTarget.form?.requestSubmit();
            }
          }}
          placeholder="Message"
          rows={1}
          disabled={disabled}
          className={cn(
            "max-h-32 min-h-9 flex-1 resize-none rounded-2xl border border-border bg-muted px-4 py-2 text-base outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50 disabled:cursor-not-allowed disabled:opacity-60",
            tooLong &&
              "border-destructive focus-visible:border-destructive focus-visible:ring-destructive/50",
          )}
        />
        <Button
          type="submit"
          size="icon-lg"
          aria-label={requestingKey ? "Requesting public key" : "Send"}
          disabled={disabled || busy || text.trim().length === 0}
          // WHY: preventing the default mousedown/pointerdown keeps focus on
          // the textarea so the mobile soft keyboard does not collapse when
          // the user taps Send. The click event still fires and submits.
          onMouseDown={(e) => {
            e.preventDefault();
          }}
          onPointerDown={(e) => {
            e.preventDefault();
          }}
          className="rounded-full h-10.5 w-10.5"
        >
          {busy ? (
            <Loader2 className="size-4 animate-spin" aria-hidden="true" />
          ) : (
            <Send className="size-4" aria-hidden="true" />
          )}
        </Button>
      </div>
    </form>
    <Dialog
      open={fallback !== null}
      onOpenChange={(open) => {
        if (!open) setFallback(null);
      }}
    >
      <DialogContent className="max-w-sm">
        <DialogHeader>
          <DialogTitle>Send without end-to-end encryption?</DialogTitle>
          <DialogDescription>
            {fallback
              ? fallbackPrompt(adapter, fallback.reason)
              : ""}
          </DialogDescription>
        </DialogHeader>
        <DialogFooter className="flex-row justify-end gap-2">
          <Button
            type="button"
            variant="ghost"
            onClick={() => {
              setFallback(null);
            }}
          >
            Cancel
          </Button>
          <Button
            type="button"
            onClick={() => {
              void confirmFallback();
            }}
          >
            Send with PSK
          </Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
    </>
  );
}

import { usePostHog } from "@posthog/react";
import { useRef, useState, type FormEvent } from "react";
import { Send } from "lucide-react";

import { cn, hapticTick } from "@/shared/lib";
import { Button, toast } from "@/shared/ui";

import { useSendMeshMessage } from "../model/use-send-mesh-message";

type SendMeshFormProps = {
  // Channel to address. Default 0 = Primary. On Landlink devices only
  // Primary works (the hook errors on other indices); on Meshtastic the
  // index routes via MeshPacket.channel.
  channelIndex?: number;
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
  disabled = false,
  disabledReason,
}: SendMeshFormProps = {}) {
  const { send, status, maxBytes } = useSendMeshMessage(channelIndex);
  const [text, setText] = useState("");
  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const posthog = usePostHog();

  const byteLength = new TextEncoder().encode(text).byteLength;
  const tooLong = byteLength > maxBytes;
  const sending = status === "sending";

  async function handleSubmit(e: FormEvent<HTMLFormElement>): Promise<void> {
    e.preventDefault();
    if (disabled || sending || text.trim().length === 0) return;
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
    const ok = await send(sent);
    if (ok) {
      posthog.capture("mesh_message_sent", { byte_length: byteLength });
      setText((cur) => (cur === sent ? "" : cur));
    }
    textareaRef.current?.focus();
  }

  const showDisabledReason = (): void => {
    if (disabled && disabledReason) {
      toast.info(disabledReason);
    }
  };

  return (
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
          aria-label="Send"
          disabled={disabled || sending || text.trim().length === 0}
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
          <Send className="size-4" aria-hidden="true" />
        </Button>
      </div>
    </form>
  );
}

import { usePostHog } from "@posthog/react";
import { Hash, Lock, Send } from "lucide-react";
import { useRef, useState, type FormEvent } from "react";
import { useNavigate } from "react-router-dom";

import { type LoraPeer } from "@/entities/lora-peer";
import { usePublicKey } from "@/entities/meshtastic-pki";
import { ROUTES } from "@/shared/config";
import { cn, hapticTick } from "@/shared/lib";
import {
  Button,
  Drawer,
  DrawerContent,
  DrawerDescription,
  DrawerHeader,
  DrawerTitle,
  toast,
} from "@/shared/ui";

import { useSendMeshMessage } from "../model/use-send-mesh-message";

type PeerDmDrawerProps = {
  peer: LoraPeer | null;
  onOpenChange: (open: boolean) => void;
};

export function PeerDmDrawer({ peer, onOpenChange }: PeerDmDrawerProps) {
  return (
    <Drawer
      open={peer !== null}
      onOpenChange={(next) => {
        if (!next) onOpenChange(false);
      }}
    >
      <DrawerContent className="pb-[max(env(safe-area-inset-bottom),0.75rem)]">
        <DrawerHeader>
          <DrawerTitle>
            {peer ? (
              <span className="font-mono text-base">{peer.nodeId}</span>
            ) : (
              "Direct message"
            )}
          </DrawerTitle>
          <DrawerDescription>
            Direct message over Primary channel.
          </DrawerDescription>
        </DrawerHeader>
        <PeerDmComposer
          key={peer?.nodeNum.toString() ?? "empty"}
          peer={peer}
          onSent={() => {
            onOpenChange(false);
          }}
        />
      </DrawerContent>
    </Drawer>
  );
}

type PeerDmComposerProps = {
  peer: LoraPeer | null;
  onSent: () => void;
};

function PeerDmComposer({ peer, onSent }: PeerDmComposerProps) {
  const { send, status, maxBytes } = useSendMeshMessage(0);
  const publicKey = usePublicKey(peer?.nodeNum ?? null);
  const navigate = useNavigate();
  const [text, setText] = useState("");
  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const posthog = usePostHog();

  const byteLength = new TextEncoder().encode(text).byteLength;
  const tooLong = byteLength > maxBytes;
  const sending = status === "sending";
  const pkiEncrypted = publicKey !== null;

  async function handleSubmit(e: FormEvent<HTMLFormElement>): Promise<void> {
    e.preventDefault();
    if (!peer || sending || text.trim().length === 0) return;
    if (tooLong) {
      toast.error(
        `Messages over ${maxBytes.toString()} bytes are not allowed`,
      );
      return;
    }
    hapticTick();
    const sent = text;
    textareaRef.current?.focus();
    const ok = await send(sent, { recipientNodeNum: peer.nodeNum });
    if (ok) {
      posthog.capture("peer_dm_sent", {
        byte_length: byteLength,
        pki_encrypted: pkiEncrypted,
      });
      setText("");
      toast.success("Message sent");
      onSent();
      const path = ROUTES.dmChat.replace(":nodeIdHex", peer.nodeId);
      void navigate(path);
    }
  }

  return (
    <form
      className="flex flex-col gap-3 px-4 pb-3"
      onSubmit={(e) => void handleSubmit(e)}
    >
      {pkiEncrypted ? (
        <div className="flex items-center gap-2 rounded-md border border-emerald-500/30 bg-emerald-500/10 px-3 py-2 text-xs text-emerald-700 dark:text-emerald-300">
          <Lock className="size-3.5 shrink-0" aria-hidden="true" />
          <span>End-to-end encrypted (PKI)</span>
        </div>
      ) : (
        <div className="flex items-start gap-2 rounded-md border border-amber-500/40 bg-amber-500/10 px-3 py-2 text-xs text-amber-700 dark:text-amber-300">
          <Hash className="mt-0.5 size-3.5 shrink-0" aria-hidden="true" />
          <span>
            Channel-encrypted only. Public key unknown; message will fall
            back to channel PSK encryption.
          </span>
        </div>
      )}
      <div className="flex items-end gap-2">
        <textarea
          ref={textareaRef}
          aria-label="Direct message text"
          value={text}
          onChange={(e) => {
            setText(e.target.value);
          }}
          onKeyDown={(e) => {
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
          rows={2}
          autoFocus
          className={cn(
            "max-h-32 min-h-9 flex-1 resize-none rounded-2xl border border-border bg-muted px-4 py-2 text-base outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50",
            tooLong &&
              "border-destructive focus-visible:border-destructive focus-visible:ring-destructive/50",
          )}
        />
        <Button
          type="submit"
          size="icon-lg"
          aria-label="Send"
          disabled={sending || text.trim().length === 0 || tooLong}
          onMouseDown={(e) => {
            e.preventDefault();
          }}
          onPointerDown={(e) => {
            e.preventDefault();
          }}
          className="h-10.5 w-10.5 rounded-full"
        >
          <Send className="size-4" aria-hidden="true" />
        </Button>
      </div>
      <div className="flex justify-end">
        <span
          className={cn(
            "text-xs tabular-nums text-muted-foreground",
            tooLong && "text-destructive",
          )}
        >
          {byteLength.toString()}/{maxBytes.toString()} bytes
        </span>
      </div>
    </form>
  );
}

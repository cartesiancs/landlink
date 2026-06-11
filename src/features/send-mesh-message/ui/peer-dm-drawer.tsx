import { usePostHog } from "@posthog/react";
import { Hash, Loader2, Lock, Send } from "lucide-react";
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
  const { send, status, error, maxBytes, adapter } = useSendMeshMessage(0);
  const publicKey = usePublicKey(peer?.nodeNum ?? null);
  const navigate = useNavigate();
  const [text, setText] = useState("");
  // Inline PSK fallback prompt. We render this in-place inside the Drawer
  // instead of opening a nested shadcn Dialog overlay because Dialog on top
  // of Drawer collides with the drawer's drag handler on mobile. When set,
  // the composer body is replaced with a confirm/cancel pair carrying the
  // pending message body.
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
  const pkiEncrypted = publicKey !== null;

  async function attemptSend(
    body: string,
    skipPkiBootstrap: boolean,
  ): Promise<boolean> {
    if (!peer) return false;
    return send(body, {
      recipientNodeNum: peer.nodeNum,
      ...(skipPkiBootstrap ? { skipPkiBootstrap: true } : {}),
    });
  }

  function onSuccess(sentBody: string): void {
    if (!peer) return;
    posthog.capture("peer_dm_sent", {
      byte_length: new TextEncoder().encode(sentBody).byteLength,
      pki_encrypted: pkiEncrypted,
    });
    setText("");
    toast.success("Message sent");
    onSent();
    const path = ROUTES.dmChat.replace(":nodeIdHex", peer.nodeId);
    void navigate(path);
  }

  async function handleSubmit(e: FormEvent<HTMLFormElement>): Promise<void> {
    e.preventDefault();
    if (!peer || busy || text.trim().length === 0) return;
    if (tooLong) {
      toast.error(
        `Messages over ${maxBytes.toString()} bytes are not allowed`,
      );
      return;
    }
    hapticTick();
    const sent = text;
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
  }

  async function confirmFallback(): Promise<void> {
    if (!fallback) return;
    const body = fallback.body;
    setFallback(null);
    const ok = await attemptSend(body, true);
    if (ok) onSuccess(body);
  }

  if (fallback) {
    return (
      <div className="flex flex-col gap-3 px-4 pb-3">
        <div className="flex flex-col gap-2 rounded-md border border-amber-500/40 bg-amber-500/10 px-3 py-3 text-xs text-amber-700 dark:text-amber-300">
          <p className="font-medium">Send without end-to-end encryption?</p>
          <p className="leading-relaxed">
            {fallbackPrompt(adapter, fallback.reason)}
          </p>
        </div>
        <div className="flex items-center justify-end gap-2">
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
        </div>
      </div>
    );
  }

  return (
    <form
      className="flex flex-col gap-3 px-4 pb-3"
      onSubmit={(e) => void handleSubmit(e)}
    >
      {requestingKey ? (
        <div className="flex items-center gap-2 rounded-md border border-sky-500/30 bg-sky-500/10 px-3 py-2 text-xs text-sky-700 dark:text-sky-300">
          <Loader2
            className="size-3.5 shrink-0 animate-spin"
            aria-hidden="true"
          />
          <span>Requesting public key from this node...</span>
        </div>
      ) : pkiEncrypted ? (
        <div className="flex items-center gap-2 rounded-md border border-emerald-500/30 bg-emerald-500/10 px-3 py-2 text-xs text-emerald-700 dark:text-emerald-300">
          <Lock className="size-3.5 shrink-0" aria-hidden="true" />
          <span>End-to-end encrypted (PKI)</span>
        </div>
      ) : (
        <div className="flex items-start gap-2 rounded-md border border-amber-500/40 bg-amber-500/10 px-3 py-2 text-xs text-amber-700 dark:text-amber-300">
          <Hash className="mt-0.5 size-3.5 shrink-0" aria-hidden="true" />
          <span>
            Public key unknown. Sending will request the peer's NodeInfo
            first; if it does not reply, we will offer a channel PSK
            fallback.
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
          disabled={busy || text.trim().length === 0 || tooLong}
          onMouseDown={(e) => {
            e.preventDefault();
          }}
          onPointerDown={(e) => {
            e.preventDefault();
          }}
          className="h-10.5 w-10.5 rounded-full"
        >
          {busy ? (
            <Loader2 className="size-4 animate-spin" aria-hidden="true" />
          ) : (
            <Send className="size-4" aria-hidden="true" />
          )}
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

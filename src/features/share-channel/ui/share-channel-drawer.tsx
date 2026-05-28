import { Check, Copy } from "lucide-react";
import { useState } from "react";

import {
  buildMeshtasticChannelUrl,
  pskToBase64,
  pskToHex,
  type Channel,
} from "@/entities/meshtastic-channel";
import { cn, hapticTick } from "@/shared/lib";
import {
  Button,
  Drawer,
  DrawerClose,
  DrawerContent,
  DrawerDescription,
  DrawerFooter,
  DrawerHeader,
  DrawerTitle,
} from "@/shared/ui";

type ShareChannelDrawerProps = {
  channel: Channel | null;
  onOpenChange: (open: boolean) => void;
};

type CopyableProps = {
  label: string;
  value: string;
  monospace?: boolean;
};

function Copyable({ label, value, monospace = false }: CopyableProps) {
  const [copied, setCopied] = useState(false);

  const handleCopy = () => {
    hapticTick();
    void navigator.clipboard.writeText(value).then(() => {
      setCopied(true);
      window.setTimeout(() => {
        setCopied(false);
      }, 1500);
    });
  };

  return (
    <div className="flex flex-col gap-1.5">
      <span className="text-xs font-medium text-muted-foreground">{label}</span>
      <div className="flex items-stretch gap-2">
        <pre
          className={cn(
            "min-w-0 flex-1 overflow-x-auto rounded-md border border-border bg-muted px-3 py-2 text-xs",
            monospace && "font-mono",
          )}
        >
          {value}
        </pre>
        <button
          type="button"
          aria-label={`Copy ${label}`}
          onClick={handleCopy}
          className="flex size-9 shrink-0 items-center justify-center self-start rounded-md border border-border text-muted-foreground hover:bg-muted hover:text-foreground"
        >
          {copied ? (
            <Check className="size-4" aria-hidden="true" />
          ) : (
            <Copy className="size-4" aria-hidden="true" />
          )}
        </button>
      </div>
    </div>
  );
}

export function ShareChannelDrawer({
  channel,
  onOpenChange,
}: ShareChannelDrawerProps) {
  const open = channel !== null;
  const url = channel ? buildMeshtasticChannelUrl(channel) : "";
  const pskBase64 = channel ? pskToBase64(channel.psk) : "";
  const pskHex = channel ? pskToHex(channel.psk) : "";
  const noKey = channel !== null && channel.psk.byteLength === 0;

  return (
    <Drawer
      open={open}
      onOpenChange={(next) => {
        if (!next) onOpenChange(false);
      }}
    >
      <DrawerContent className="pb-[max(env(safe-area-inset-bottom),0.75rem)]">
        <DrawerHeader>
          <DrawerTitle>Share channel</DrawerTitle>
          <DrawerDescription>
            {channel
              ? `Anyone with this URL can join "${channel.name}" on a Meshtastic device.`
              : ""}
          </DrawerDescription>
        </DrawerHeader>
        <div className="flex flex-col gap-4 px-4 pb-2">
          {noKey ? (
            <p className="rounded-md border border-dashed border-border px-3 py-3 text-center text-xs text-muted-foreground">
              The device hasn't sent the channel key yet. Try reconnecting,
              then reopen this sheet.
            </p>
          ) : (
            <>
              <Copyable
                label="Meshtastic URL"
                value={url}
                monospace
              />
              <Copyable label="PSK (base64)" value={pskBase64} monospace />
              <Copyable label="PSK (hex)" value={pskHex} monospace />
            </>
          )}
        </div>
        <DrawerFooter>
          <DrawerClose asChild>
            <Button variant="ghost" size="lg">
              Close
            </Button>
          </DrawerClose>
        </DrawerFooter>
      </DrawerContent>
    </Drawer>
  );
}

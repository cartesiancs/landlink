import { useState, type FormEvent } from "react";

import { cn, hapticTick } from "@/shared/lib";
import {
  Button,
  Dialog,
  DialogContent,
  DialogDescription,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from "@/shared/ui";

import { useImportChannel } from "../model/use-import-channel";

type ImportChannelDialogProps = {
  open: boolean;
  onOpenChange: (open: boolean) => void;
};

export function ImportChannelDialog({
  open,
  onOpenChange,
}: ImportChannelDialogProps) {
  const { importChannel, error, maxNameBytes, canImport, reset } =
    useImportChannel();
  const [name, setName] = useState("");
  const [channelKey, setChannelKey] = useState("");

  const handleOpenChange = (next: boolean) => {
    if (!next) {
      setName("");
      setChannelKey("");
      reset();
    }
    onOpenChange(next);
  };

  const byteLength = new TextEncoder().encode(name).byteLength;
  const tooLong = byteLength > maxNameBytes;
  const canSubmit =
    canImport &&
    !tooLong &&
    name.trim().length > 0 &&
    channelKey.trim().length > 0;

  function handleSubmit(e: FormEvent<HTMLFormElement>): void {
    e.preventDefault();
    if (!canSubmit) return;
    hapticTick();
    void (async () => {
      const ok = await importChannel(name, channelKey);
      if (ok) onOpenChange(false);
    })();
  }

  return (
    <Dialog open={open} onOpenChange={handleOpenChange}>
      <DialogContent>
        <DialogHeader>
          <DialogTitle>Import channel</DialogTitle>
          <DialogDescription>
            Enter the channel name and its PSK to add it to this device.
          </DialogDescription>
        </DialogHeader>
        <form onSubmit={handleSubmit} className="flex flex-col gap-3">
          <label className="flex flex-col gap-1.5">
            <span className="text-xs font-medium text-muted-foreground">
              Channel name
            </span>
            <input
              type="text"
              autoFocus
              value={name}
              onChange={(e) => {
                setName(e.target.value);
              }}
              placeholder="e.g. Family"
              maxLength={32}
              className={cn(
                "h-10 rounded-md border border-border bg-muted px-3 text-base outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50",
                tooLong &&
                  "border-destructive focus-visible:border-destructive focus-visible:ring-destructive/50",
              )}
            />
            <span
              className={cn(
                "text-[11px] text-muted-foreground",
                tooLong && "text-destructive",
              )}
            >
              {byteLength.toString()}/{maxNameBytes.toString()} bytes
            </span>
          </label>
          <label className="flex flex-col gap-1.5">
            <span className="text-xs font-medium text-muted-foreground">
              Channel key
            </span>
            <textarea
              value={channelKey}
              onChange={(e) => {
                setChannelKey(e.target.value);
              }}
              placeholder="base64 or hex"
              rows={3}
              className={cn(
                "resize-none rounded-md border border-border bg-muted px-3 py-2 font-mono text-xs outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50",
              )}
            />
          </label>
          {error ? (
            <p className="text-xs text-destructive" role="alert">
              {error}
            </p>
          ) : null}
          <DialogFooter className="mt-2">
            <Button type="submit" disabled={!canSubmit}>
              Import channel
            </Button>
          </DialogFooter>
        </form>
      </DialogContent>
    </Dialog>
  );
}

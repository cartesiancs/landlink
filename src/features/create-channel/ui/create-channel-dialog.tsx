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

import { useCreateChannel } from "../model/use-create-channel";

type CreateChannelDialogProps = {
  open: boolean;
  onOpenChange: (open: boolean) => void;
};

export function CreateChannelDialog({
  open,
  onOpenChange,
}: CreateChannelDialogProps) {
  const { create, error, maxNameBytes, canCreate, reset } = useCreateChannel();
  const [name, setName] = useState("");

  // Reset local + hook state on close. Doing it in the open-change handler
  // (rather than a useEffect on `open`) keeps the cleanup off the render path
  // and away from the react-hooks/set-state-in-effect lint rule.
  const handleOpenChange = (next: boolean) => {
    if (!next) {
      setName("");
      reset();
    }
    onOpenChange(next);
  };

  const byteLength = new TextEncoder().encode(name).byteLength;
  const tooLong = byteLength > maxNameBytes;

  function handleSubmit(e: FormEvent<HTMLFormElement>): void {
    e.preventDefault();
    if (!canCreate || tooLong || name.trim().length === 0) return;
    hapticTick();
    const ok = create(name);
    if (ok) onOpenChange(false);
  }

  return (
    <Dialog open={open} onOpenChange={handleOpenChange}>
      <DialogContent>
        <DialogHeader>
          <DialogTitle>New channel</DialogTitle>
          <DialogDescription>
            Each channel has its own encryption key. The PSK is generated for
            you. You can share the channel later via a QR or link.
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
          {error ? (
            <p className="text-xs text-destructive" role="alert">
              {error}
            </p>
          ) : null}
          <DialogFooter className="mt-2">
            <Button
              type="submit"
              disabled={
                !canCreate || tooLong || name.trim().length === 0
              }
            >
              Create channel
            </Button>
          </DialogFooter>
        </form>
      </DialogContent>
    </Dialog>
  );
}

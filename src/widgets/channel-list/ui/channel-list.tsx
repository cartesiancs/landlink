import { useState } from "react";

import {
  removeSecondary,
  useChannels,
  type Channel,
} from "@/entities/meshtastic-channel";
import { useLandlinkDevice } from "@/entities/landlink-device";
import { hapticTick } from "@/shared/lib";
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

import { ChannelRow } from "./channel-row";

export function ChannelList() {
  const channels = useChannels();
  const device = useLandlinkDevice();
  const [pendingDelete, setPendingDelete] = useState<Channel | null>(null);

  if (!channels) {
    return (
      <div className="flex flex-col items-center justify-center gap-3 py-12 text-center">
        <p className="text-sm text-muted-foreground">No device connected.</p>
        <p className="max-w-[280px] text-xs text-muted-foreground">
          Connect a device to view and manage its channels.
        </p>
      </div>
    );
  }

  const confirmDelete = () => {
    if (!pendingDelete || !device) {
      setPendingDelete(null);
      return;
    }
    hapticTick();
    removeSecondary(device.deviceId, pendingDelete.index);
    setPendingDelete(null);
  };

  return (
    <>
      <ul className="flex flex-col gap-2">
        {channels.map((c) => (
          <ChannelRow
            key={c.index}
            channel={c}
            onRequestDelete={setPendingDelete}
          />
        ))}
      </ul>
      <Drawer
        open={pendingDelete !== null}
        onOpenChange={(open) => {
          if (!open) setPendingDelete(null);
        }}
      >
        <DrawerContent className="pb-[max(env(safe-area-inset-bottom),0.75rem)]">
          <DrawerHeader>
            <DrawerTitle>Delete this channel?</DrawerTitle>
            <DrawerDescription>
              {pendingDelete
                ? `"${pendingDelete.name}" will be removed from this device. Messages on this channel will no longer be visible.`
                : "This channel will be removed from this device."}
            </DrawerDescription>
          </DrawerHeader>
          <DrawerFooter>
            <Button variant="destructive" size="lg" onClick={confirmDelete}>
              Delete channel
            </Button>
            <DrawerClose asChild>
              <Button
                variant="ghost"
                size="lg"
                onClick={() => {
                  hapticTick();
                }}
              >
                Cancel
              </Button>
            </DrawerClose>
          </DrawerFooter>
        </DrawerContent>
      </Drawer>
    </>
  );
}

import { useState } from "react";

import {
  landlinkChannelDelete,
  useLandlinkDevice,
} from "@/entities/landlink-device";
import {
  useChannels,
  type Channel,
} from "@/entities/meshtastic-channel";
import { findDevice, useRegisteredDevices } from "@/entities/registered-device";
import { ShareChannelDrawer } from "@/features/share-channel";
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
  const registeredDevices = useRegisteredDevices();
  const registered = device
    ? findDevice(registeredDevices, device.deviceId)
    : null;
  // Meshtastic channel rows are read-only locally — deleting would only drop
  // the row from the in-memory mirror, not from the device's NVS, so the row
  // would reappear on the next FromRadio refresh.
  const rowsDeletable = registered?.protocol !== "meshtastic";
  const [pendingDelete, setPendingDelete] = useState<Channel | null>(null);
  const [pendingShare, setPendingShare] = useState<Channel | null>(null);

  if (!channels) {
    return (
      <div className="flex flex-col items-center justify-center gap-3 py-12 text-center">
        <p className="text-sm text-muted-foreground">No device connected.</p>
      </div>
    );
  }

  const confirmDelete = () => {
    if (!pendingDelete || !device) {
      setPendingDelete(null);
      return;
    }
    hapticTick();
    const index = pendingDelete.index;
    setPendingDelete(null);
    // Fire-and-forget: the sync feature observes the CHANNEL_RESULT EVT
    // and removes the slot from the local cache. If the device rejects
    // the delete (e.g. busy on another transaction), the row stays put
    // and the user can retry.
    void landlinkChannelDelete(index).catch((err: unknown) => {
      console.warn("[channels] delete failed", index, err);
    });
  };

  return (
    <>
      <ul className="flex flex-col gap-2">
        {channels.map((c) => (
          <ChannelRow
            key={c.index}
            channel={c}
            deletable={rowsDeletable}
            onRequestDelete={setPendingDelete}
            onRequestShare={setPendingShare}
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
      <ShareChannelDrawer
        channel={pendingShare}
        onOpenChange={(open) => {
          if (!open) setPendingShare(null);
        }}
      />
    </>
  );
}

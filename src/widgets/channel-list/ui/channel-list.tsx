import { useState } from "react";
import { useNavigate } from "react-router-dom";

import {
  landlinkChannelDelete,
  useLandlinkDevice,
} from "@/entities/landlink-device";
import {
  useChannels,
  type Channel,
} from "@/entities/meshtastic-channel";
import {
  findDevice,
  useActiveDeviceId,
  useRegisteredDevices,
} from "@/entities/registered-device";
import { ShareChannelDrawer } from "@/features/share-channel";
import { ROUTES } from "@/shared/config";
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
  const navigate = useNavigate();
  const channels = useChannels();
  const device = useLandlinkDevice();
  const activeDeviceId = useActiveDeviceId();
  const registeredDevices = useRegisteredDevices();
  const registered = activeDeviceId
    ? findDevice(registeredDevices, activeDeviceId)
    : null;
  const isConnected = device?.status === "connected";
  // Channel rows stay deletable only while the device is online and speaks
  // the Landlink protocol — Meshtastic channels are managed via the official
  // app, and offline deletes would only drop the local mirror while the
  // device's NVS still holds the slot, so the row would reappear on the
  // next sync.
  const rowsDeletable = isConnected && registered?.protocol !== "meshtastic";
  const [pendingDelete, setPendingDelete] = useState<Channel | null>(null);
  const [pendingShare, setPendingShare] = useState<Channel | null>(null);

  if (channels === null) {
    return (
      <div className="flex flex-col items-center justify-center gap-3 py-12 text-center">
        <p className="text-sm text-muted-foreground">
          {activeDeviceId === null
            ? "Pair a device to view channels."
            : "Connect your device to load channels."}
        </p>
        <Button
          variant="outline"
          onClick={() => {
            hapticTick();
            void navigate(ROUTES.connectBluetooth);
          }}
        >
          {activeDeviceId === null ? "Pair a device" : "Connect a device"}
        </Button>
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

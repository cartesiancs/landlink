import { Users } from "lucide-react";
import { Capacitor } from "@capacitor/core";
import { useState } from "react";

import { type LoraPeer, useLoraPeers } from "@/entities/lora-peer";
import { PeerDmDrawer } from "@/features/send-mesh-message";
import { cn, hapticTick } from "@/shared/lib";
import {
  Button,
  Sheet,
  SheetContent,
  SheetDescription,
  SheetHeader,
  SheetTitle,
  SheetTrigger,
} from "@/shared/ui";

import { NodeListRow } from "./node-list-row";

const IS_NATIVE_APP = Capacitor.isNativePlatform();

export function NodeListSheet() {
  const peers = useLoraPeers();
  const [dmPeer, setDmPeer] = useState<LoraPeer | null>(null);

  return (
    <Sheet>
      <SheetTrigger asChild>
        <Button
          variant="ghost"
          size="icon"
          aria-label="Show node list"
          onClick={() => {
            hapticTick();
          }}
        >
          <Users aria-hidden="true" />
        </Button>
      </SheetTrigger>
      <SheetContent side="right" className="gap-0">
        <SheetHeader
          className={cn(
            IS_NATIVE_APP
              ? "pt-[calc(max(env(safe-area-inset-top),1rem)+18px)]"
              : "pt-[max(env(safe-area-inset-top),1rem)]",
          )}
        >
          <SheetTitle>Nodes</SheetTitle>
          <SheetDescription></SheetDescription>
        </SheetHeader>
        {peers.length === 0 ? (
          <div className="flex flex-1 items-center justify-center px-4 pb-6">
            <p className="text-sm text-muted-foreground">No nodes heard yet.</p>
          </div>
        ) : (
          <ul
            className="flex flex-col gap-2 overflow-y-auto px-4 pb-4"
            style={{
              paddingBottom: "calc(max(env(safe-area-inset-bottom), 1rem))",
            }}
          >
            {peers.map((peer) => (
              <NodeListRow
                key={peer.nodeId}
                peer={peer}
                onStartDm={setDmPeer}
              />
            ))}
          </ul>
        )}
      </SheetContent>
      <PeerDmDrawer
        peer={dmPeer}
        onOpenChange={(open) => {
          if (!open) setDmPeer(null);
        }}
      />
    </Sheet>
  );
}

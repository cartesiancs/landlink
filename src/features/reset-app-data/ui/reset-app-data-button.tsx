import { useState } from "react";
import { useNavigate } from "react-router-dom";

import { detachLandlinkClient, useLandlinkDevice } from "@/entities/landlink-device";
import { ROUTES } from "@/shared/config";
import { hapticTick } from "@/shared/lib";
import {
  Button,
  Drawer,
  DrawerContent,
  DrawerDescription,
  DrawerHeader,
  DrawerTitle,
  toast,
} from "@/shared/ui";

import { resetAppData } from "../model/reset-app-data";

export function ResetAppDataButton() {
  const [open, setOpen] = useState(false);
  const live = useLandlinkDevice();
  const navigate = useNavigate();

  const handleConfirm = () => {
    if (live !== null) {
      void detachLandlinkClient(live.deviceId);
    }
    resetAppData();
    setOpen(false);
    toast.success("All app data has been reset.");
    void navigate(ROUTES.home, { viewTransition: true });
  };

  return (
    <>
      <Button
        variant="destructive"
        className="w-full"
        onClick={() => {
          hapticTick();
          setOpen(true);
        }}
      >
        Reset all data
      </Button>
      <Drawer open={open} onOpenChange={setOpen}>
        <DrawerContent>
          <DrawerHeader>
            <DrawerTitle>Reset all data?</DrawerTitle>
            <DrawerDescription>
              This removes every registered device and clears app preferences.
              The action cannot be undone.
            </DrawerDescription>
          </DrawerHeader>
          <div className="flex flex-col gap-2 px-4 pb-[max(env(safe-area-inset-bottom),1.5rem)]">
            <Button
              variant="destructive"
              className="w-full"
              onClick={() => {
                hapticTick();
                handleConfirm();
              }}
            >
              Reset everything
            </Button>
            <Button
              variant="outline"
              className="w-full"
              onClick={() => {
                setOpen(false);
              }}
            >
              Cancel
            </Button>
          </div>
        </DrawerContent>
      </Drawer>
    </>
  );
}

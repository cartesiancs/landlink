import { usePostHog } from "@posthog/react";
import { Plus } from "lucide-react";

import { registerDevice } from "@/entities/registered-device";
import { hapticTick } from "@/shared/lib";
import { Button, toast } from "@/shared/ui";

import { createMockDevice } from "../model/create-mock-device";

export function RegisterMockDeviceButton() {
  const posthog = usePostHog();

  return (
    <Button
      variant="outline"
      className="w-full justify-start gap-2"
      onClick={() => {
        hapticTick();
        const draft = createMockDevice();
        registerDevice({
          id: draft.id,
          name: draft.name,
          source: "mock",
          pingMs: draft.pingMs,
          signalDbm: draft.signalDbm,
        });
        posthog.capture("mock_device_registered", { device_name: draft.name });
        toast.success(`Mock device "${draft.name}" added.`);
      }}
    >
      <Plus className="size-4" aria-hidden="true" />
      Register mock device
    </Button>
  );
}

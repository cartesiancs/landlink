import { Check, ChevronsUpDown } from "lucide-react";

import {
  setSelectedDeviceId,
  useActiveDeviceId,
  useRegisteredDevices,
} from "@/entities/registered-device";
import { hapticTick } from "@/shared/lib";
import {
  Button,
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/shared/ui";

// Lets the user pick which registered device's cached channels appear on the
// Channels page when no BLE connection is active. Callers (channels-page)
// decide whether to mount it — it only makes sense when 2+ devices are
// registered and nothing is currently connected.
export function ActiveDevicePicker() {
  const registered = useRegisteredDevices();
  const activeId = useActiveDeviceId();
  const active = registered.find((d) => d.id === activeId) ?? null;
  const label = active?.name ?? "Select device";

  return (
    <DropdownMenu>
      <DropdownMenuTrigger asChild>
        <Button
          variant="outline"
          size="sm"
          className="h-8 gap-1.5 px-2.5 text-xs font-normal"
          aria-label="Select device to view channels"
        >
          <span className="max-w-[10rem] truncate">{label}</span>
          <ChevronsUpDown className="size-3.5 opacity-60" aria-hidden />
        </Button>
      </DropdownMenuTrigger>
      <DropdownMenuContent className="min-w-[12rem]" align="end">
        {registered.map((d) => {
          const isActive = d.id === activeId;
          return (
            <DropdownMenuItem
              key={d.id}
              onSelect={() => {
                hapticTick();
                setSelectedDeviceId(d.id);
              }}
            >
              <span className="flex-1 truncate">{d.name}</span>
              {isActive ? (
                <Check className="ml-2 size-3.5 opacity-70" aria-hidden />
              ) : null}
            </DropdownMenuItem>
          );
        })}
      </DropdownMenuContent>
    </DropdownMenu>
  );
}

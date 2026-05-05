import { type ReactNode, useState } from "react";
import { useLocation, useNavigate } from "react-router-dom";
import { Capacitor } from "@capacitor/core";

import { Button, SlideSwitch } from "@/shared/ui";
import {
  cn,
  hapticTick,
  StepActionProvider,
  useStepAction,
} from "@/shared/lib";
import { ROUTES, type RoutePath } from "@/shared/config";
import { useRegisteredDevices } from "@/entities/registered-device";
import { AppHeader } from "@/widgets/app-header";
import { NavigationSidebar } from "@/widgets/navigation-sidebar";
import { SupportDrawer } from "@/widgets/support-drawer";
import { isBlePairingSupported } from "@/features/bluetooth-pairing";
import { ConnectBluetoothPage } from "@/pages/connect-bluetooth";
import { ConnectWifiPage } from "@/pages/connect-wifi";
import { ConnectingPage } from "@/pages/connecting";
import { NotFoundPage } from "@/pages/not-found";
import { UnsupportedDevicePage } from "@/pages/unsupported-device";

import { HomeOrListsRedirect } from "./home-redirect-guard";

type StepPath =
  | typeof ROUTES.home
  | typeof ROUTES.connectBluetooth
  | typeof ROUTES.connectUnsupported
  | typeof ROUTES.connectWifi
  | typeof ROUTES.connecting;

type StepMeta = {
  label: string;
  next: RoutePath;
};

const STEP_META: Record<StepPath, StepMeta> = {
  [ROUTES.home]: { label: "Get started", next: ROUTES.connectBluetooth },
  [ROUTES.connectBluetooth]: { label: "Connect", next: ROUTES.connectWifi },
  [ROUTES.connectUnsupported]: { label: "Back", next: ROUTES.home },
  [ROUTES.connectWifi]: { label: "Connect", next: ROUTES.connecting },
  [ROUTES.connecting]: { label: "Cancel", next: ROUTES.home },
};

const STEP_PATHS = new Set<string>([
  ROUTES.home,
  ROUTES.connectBluetooth,
  ROUTES.connectUnsupported,
  ROUTES.connectWifi,
  ROUTES.connecting,
]);

function isStepPath(pathname: string): pathname is StepPath {
  return STEP_PATHS.has(pathname);
}

function renderStep(pathname: StepPath): ReactNode {
  switch (pathname) {
    case ROUTES.home:
      return <HomeOrListsRedirect />;
    case ROUTES.connectBluetooth:
      return <ConnectBluetoothPage />;
    case ROUTES.connectUnsupported:
      return <UnsupportedDevicePage />;
    case ROUTES.connectWifi:
      return <ConnectWifiPage />;
    case ROUTES.connecting:
      return <ConnectingPage />;
  }
}

const IS_NATIVE_APP = Capacitor.isNativePlatform();

type StepCtaProps = {
  defaultLabel: string;
  defaultOnAction: () => void;
};

function StepCta({ defaultLabel, defaultOnAction }: StepCtaProps) {
  const override = useStepAction();
  const label = override.label ?? defaultLabel;
  const onAction = override.onAction ?? defaultOnAction;
  const disabled = override.disabled ?? false;
  const pending = override.pending ?? false;

  const handleClick = () => {
    if (disabled) return;
    hapticTick();
    void onAction();
  };

  return (
    <div className="relative z-30">
      <div
        className={cn(
          "bg-background/90 px-4 pt-3 backdrop-blur supports-backdrop-filter:bg-background/70",
          IS_NATIVE_APP ? "pb-3" : "pb-0",
        )}
      >
        <Button
          size="lg"
          disabled={disabled}
          aria-busy={pending || undefined}
          className="h-12 w-full text-base transition-opacity duration-300 ease-out data-[busy=true]:opacity-80"
          data-busy={pending || undefined}
          onClick={handleClick}
        >
          <SlideSwitch contentKey={label}>{label}</SlideSwitch>
        </Button>
      </div>
      {/* Solid strip covering the iOS safe-area-inset-bottom so Safari's URL bar
          tint can never sample the translucent layer or the dark CTA button. */}
      <div
        aria-hidden
        className="h-[max(env(safe-area-inset-bottom),12px)] bg-background"
      />
    </div>
  );
}

export function AppLayout() {
  const location = useLocation();
  const navigate = useNavigate();
  const devices = useRegisteredDevices();
  const [sidebarOpen, setSidebarOpen] = useState(false);
  const [supportOpen, setSupportOpen] = useState(false);

  if (!isStepPath(location.pathname)) {
    return <NotFoundPage />;
  }

  const pathname = location.pathname;
  const { label, next } = STEP_META[pathname];

  const handleAdvance = () => {
    if (pathname === ROUTES.home) {
      // WHY: skip the connect flow entirely when the user already has devices —
      // pairing again from home is not the intended path; sending them to the
      // list avoids a brief connect-bluetooth flash before HomeOrListsRedirect
      // would otherwise bounce them back.
      if (devices.length > 0) {
        void navigate(ROUTES.lists, { viewTransition: true });
        return;
      }
      // WHY: Home → Bluetooth is the only gating transition; when the browser
      // can't do Web Bluetooth, skip the pairing step so the user never sees
      // it flash.
      if (!isBlePairingSupported()) {
        void navigate(ROUTES.connectUnsupported);
        return;
      }
    }
    void navigate(next);
  };

  return (
    <div className="mx-auto flex h-dvh w-full max-w-[430px] flex-col bg-background">
      <AppHeader
        onMenuOpen={() => {
          setSidebarOpen(true);
        }}
        onSupportOpen={() => {
          setSupportOpen(true);
        }}
      />

      <StepActionProvider>
        <SlideSwitch contentKey={pathname} className="min-h-0 flex-1">
          {renderStep(pathname)}
        </SlideSwitch>

        <StepCta defaultLabel={label} defaultOnAction={handleAdvance} />
      </StepActionProvider>

      <NavigationSidebar open={sidebarOpen} onOpenChange={setSidebarOpen} />
      <SupportDrawer open={supportOpen} onOpenChange={setSupportOpen} />
    </div>
  );
}

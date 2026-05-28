import { usePostHog } from "@posthog/react";
import { useEffect } from "react";
import { Capacitor } from "@capacitor/core";
import {
  createBrowserRouter,
  createHashRouter,
  Outlet,
  RouterProvider,
  useLocation,
  type RouteObject,
} from "react-router-dom";

import { AboutPage } from "@/pages/about";
import { ChannelChatPage } from "@/pages/channel-chat";
import { ChannelsPage } from "@/pages/channels";
import { DeviceDashboardPage } from "@/pages/device-dashboard";
import { ErrorPage } from "@/pages/error";
import { FaqPage } from "@/pages/faq";
import { HardwareSetupPage } from "@/pages/hardware-setup";
import { LandlinkFirmwarePage } from "@/pages/landlink-firmware";
import { LandlinkModuleIPage } from "@/pages/landlink-module-i";
import { LandlinkModuleIBuyPage } from "@/pages/landlink-module-i-buy";
import { LandlinkOnePage } from "@/pages/landlink-one";
import { ListsPage } from "@/pages/lists";
import { PrivacyPage } from "@/pages/privacy";
import { SettingsPage } from "@/pages/settings";
import { SettingsDebugPage } from "@/pages/settings-debug";
import { SettingsProtocolPage } from "@/pages/settings-protocol";
import { SettingsResetPage } from "@/pages/settings-reset";
import { SettingsThemePage } from "@/pages/settings-theme";
import { TermsPage } from "@/pages/terms";
import { ROUTES } from "@/shared/config";
import { AppLayout } from "@/app/layout/app-layout";

function PostHogPageTracker() {
  const location = useLocation();
  const posthog = usePostHog();

  useEffect(() => {
    posthog.capture("$pageview");
  }, [location, posthog]);

  return <Outlet />;
}

// WHY: a parent route with errorElement (and no element of its own) lets the
// custom ErrorPage handle thrown errors from any child route without altering
// their layout. Default behavior renders react-router's plain "Unexpected
// Application Error!" message which we don't want users to see.
const routes: RouteObject[] = [
  {
    element: <PostHogPageTracker />,
    errorElement: <ErrorPage />,
    children: [
      { path: ROUTES.about, element: <AboutPage /> },
      { path: ROUTES.faq, element: <FaqPage /> },
      { path: ROUTES.landlinkOne, element: <LandlinkOnePage /> },
      { path: ROUTES.landlinkModuleI, element: <LandlinkModuleIPage /> },
      { path: ROUTES.landlinkModuleIBuy, element: <LandlinkModuleIBuyPage /> },
      { path: ROUTES.landlinkFirmware, element: <LandlinkFirmwarePage /> },
      { path: ROUTES.hardwareSetup, element: <HardwareSetupPage /> },
      { path: ROUTES.privacy, element: <PrivacyPage /> },
      { path: ROUTES.terms, element: <TermsPage /> },
      { path: ROUTES.lists, element: <ListsPage /> },
      { path: ROUTES.deviceDashboard, element: <DeviceDashboardPage /> },
      { path: ROUTES.channels, element: <ChannelsPage /> },
      { path: ROUTES.channelChat, element: <ChannelChatPage /> },
      { path: ROUTES.settings, element: <SettingsPage /> },
      { path: ROUTES.settingsTheme, element: <SettingsThemePage /> },
      { path: ROUTES.settingsDebug, element: <SettingsDebugPage /> },
      { path: ROUTES.settingsReset, element: <SettingsResetPage /> },
      { path: ROUTES.settingsProtocol, element: <SettingsProtocolPage /> },
      { path: ROUTES.error, element: <ErrorPage /> },
      { path: "*", element: <AppLayout /> },
    ],
  },
];

// WHY: on Capacitor iOS the app is served from capacitor://localhost with no
// server-side routing, so history-based routes 404 on reload. Hash routing
// side-steps this; the browser build keeps clean URLs.
const router = Capacitor.isNativePlatform()
  ? createHashRouter(routes)
  : createBrowserRouter(routes);

export function AppRouter() {
  return <RouterProvider router={router} />;
}

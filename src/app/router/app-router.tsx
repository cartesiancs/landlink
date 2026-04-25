import { Capacitor } from "@capacitor/core";
import {
  createBrowserRouter,
  createHashRouter,
  RouterProvider,
  type RouteObject,
} from "react-router-dom";

import { AboutPage } from "@/pages/about";
import { FaqPage } from "@/pages/faq";
import { HardwareSetupPage } from "@/pages/hardware-setup";
import { LandlinkModuleIPage } from "@/pages/landlink-module-i";
import { PrivacyPage } from "@/pages/privacy";
import { PurchasePage } from "@/pages/purchase";
import { TermsPage } from "@/pages/terms";
import { ROUTES } from "@/shared/config";
import { AppLayout } from "@/app/layout/app-layout";

const routes: RouteObject[] = [
  { path: ROUTES.about, element: <AboutPage /> },
  { path: ROUTES.faq, element: <FaqPage /> },
  { path: ROUTES.purchase, element: <PurchasePage /> },
  { path: ROUTES.landlinkModuleI, element: <LandlinkModuleIPage /> },
  { path: ROUTES.hardwareSetup, element: <HardwareSetupPage /> },
  { path: ROUTES.privacy, element: <PrivacyPage /> },
  { path: ROUTES.terms, element: <TermsPage /> },
  { path: "*", element: <AppLayout /> },
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

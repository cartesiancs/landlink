import { createBrowserRouter, RouterProvider } from "react-router-dom";

import { AboutPage } from "@/pages/about";
import { FaqPage } from "@/pages/faq";
import { HardwareSetupPage } from "@/pages/hardware-setup";
import { PurchasePage } from "@/pages/purchase";
import { ROUTES } from "@/shared/config";
import { AppLayout } from "@/app/layout/app-layout";

const router = createBrowserRouter([
  { path: ROUTES.about, element: <AboutPage /> },
  { path: ROUTES.faq, element: <FaqPage /> },
  { path: ROUTES.purchase, element: <PurchasePage /> },
  { path: ROUTES.hardwareSetup, element: <HardwareSetupPage /> },
  { path: "*", element: <AppLayout /> },
]);

export function AppRouter() {
  return <RouterProvider router={router} />;
}

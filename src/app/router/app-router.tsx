import { createBrowserRouter, RouterProvider } from "react-router-dom";

import { AboutPage } from "@/pages/about";
import { ROUTES } from "@/shared/config";
import { AppLayout } from "@/app/layout/app-layout";

const router = createBrowserRouter([
  { path: ROUTES.about, element: <AboutPage /> },
  { path: "*", element: <AppLayout /> },
]);

export function AppRouter() {
  return <RouterProvider router={router} />;
}

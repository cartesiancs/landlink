import { createBrowserRouter, RouterProvider } from "react-router-dom";

import { HomePage } from "@/pages/home";
import { AboutPage } from "@/pages/about";
import { NotFoundPage } from "@/pages/not-found";
import { ROUTES } from "@/shared/config";

const router = createBrowserRouter([
  { path: ROUTES.home, element: <HomePage /> },
  { path: ROUTES.about, element: <AboutPage /> },
  { path: ROUTES.notFound, element: <NotFoundPage /> },
]);

export function AppRouter() {
  return <RouterProvider router={router} />;
}

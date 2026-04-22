import { Link } from "react-router-dom";

import { Button } from "@/shared/ui";
import { ROUTES } from "@/shared/config";

export function NotFoundPage() {
  return (
    <main className="flex min-h-dvh flex-col items-center justify-center gap-4">
      <h1 className="text-2xl font-semibold">404 Not Found</h1>
      <Button variant="ghost" asChild>
        <Link to={ROUTES.home}>Back to Home</Link>
      </Button>
    </main>
  );
}

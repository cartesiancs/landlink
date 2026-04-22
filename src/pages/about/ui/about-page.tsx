import { Link } from "react-router-dom";

import { Button } from "@/shared/ui";
import { ROUTES } from "@/shared/config";

export function AboutPage() {
  return (
    <main className="flex min-h-dvh flex-col items-center justify-center gap-4">
      <h1 className="text-2xl font-semibold">About</h1>
      <Button variant="outline" asChild>
        <Link to={ROUTES.home}>Back to Home</Link>
      </Button>
    </main>
  );
}

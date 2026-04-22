import { Link } from "react-router-dom";

import { Button } from "@/shared/ui";
import { ROUTES } from "@/shared/config";

export function HomePage() {
  return (
    <main className="flex min-h-dvh flex-col items-center justify-center gap-4">
      <h1 className="text-2xl font-semibold">Home</h1>
      <Button asChild>
        <Link to={ROUTES.about}>Go to About</Link>
      </Button>
    </main>
  );
}

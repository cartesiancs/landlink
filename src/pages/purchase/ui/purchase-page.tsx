import { ChevronLeft, Clock } from "lucide-react";
import { Link } from "react-router-dom";

import { ROUTES } from "@/shared/config";

export function PurchasePage() {
  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <header className="sticky top-0 z-10 flex items-center gap-2 bg-background/90 px-4 ps-1 pt-[max(env(safe-area-inset-top),0.75rem)] pb-3 backdrop-blur supports-backdrop-filter:bg-background/70">
        <Link
          to={ROUTES.home}
          viewTransition
          className="flex size-9 items-center justify-center rounded-md hover:bg-muted"
          aria-label="Back to Home"
        >
          <ChevronLeft className="size-5" aria-hidden="true" />
        </Link>
        <h1 className="text-base font-medium">Purchase</h1>
      </header>

      <section className="flex flex-1 flex-col items-center justify-center gap-4 px-4 pb-10 text-center">
        <div className="flex size-12 items-center justify-center rounded-full bg-muted">
          <Clock className="size-5 text-foreground" aria-hidden="true" />
        </div>
        <h2 className="font-display text-2xl leading-tight tracking-tight">
          Not ready yet
        </h2>
        <p className="max-w-xs text-sm leading-relaxed text-muted-foreground">
          Purchasing isn't available at the moment. We're still preparing this
          experience — check back soon.
        </p>
      </section>
    </main>
  );
}

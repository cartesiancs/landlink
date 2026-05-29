import { Mail, MessageCircleQuestion } from "lucide-react";
import { Link } from "react-router-dom";

import { ROUTES } from "@/shared/config";
import { BackButton } from "@/shared/ui";

export function SupportPage() {
  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <header className="sticky top-0 bg-background">
        <div
          aria-hidden
          className="h-[env(safe-area-inset-top)] bg-background"
        />
        <div className="relative flex h-14 items-center justify-center px-3">
          <div className="absolute left-1 top-1/2 -translate-y-1/2">
            <BackButton fallback={ROUTES.home} aria-label="Go back" />
          </div>
          <h1 className="text-base font-medium">Support</h1>
        </div>
      </header>

      <section className="flex flex-col gap-2 px-4 pt-2">
        <a
          href="mailto:jun@cartesiancs.com"
          className="flex items-center gap-3 rounded-md border border-border px-4 py-3 text-sm hover:bg-muted"
        >
          <Mail className="size-4 text-muted-foreground" aria-hidden="true" />
          Email support
        </a>
        <Link
          to={ROUTES.faq}
          viewTransition
          className="flex items-center gap-3 rounded-md border border-border px-4 py-3 text-sm hover:bg-muted"
        >
          <MessageCircleQuestion
            className="size-4 text-muted-foreground"
            aria-hidden="true"
          />
          Read the FAQ
        </Link>
      </section>
    </main>
  );
}

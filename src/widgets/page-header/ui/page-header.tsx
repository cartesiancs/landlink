import type { ReactNode } from "react";

import { BackButton } from "@/shared/ui";

type PageHeaderProps = {
  title: ReactNode;
  fallback: string;
  backLabel?: string;
  children?: ReactNode;
};

export function PageHeader({
  title,
  fallback,
  backLabel = "Go back",
  children,
}: PageHeaderProps) {
  return (
    <header className="sticky top-0 z-10 [view-transition-name:app-header]">
      {/* WHY: opaque safe-area strip mirrors AppHeader so PageHeader content
          sits at the same vertical center across all pages — without it, the
          notch absorbs single-row padding and pulls content ~10px higher. */}
      <div aria-hidden className="h-[env(safe-area-inset-top)] bg-background" />
      <div className="flex h-14 items-center gap-2 bg-background ps-1 px-3">
        <BackButton fallback={fallback} aria-label={backLabel} />
        <h1 className="flex-1 truncate text-base font-medium">{title}</h1>
        {children}
      </div>
    </header>
  );
}

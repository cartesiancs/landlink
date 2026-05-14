import { type ReactNode, useEffect, useState } from "react";

import { BackButton } from "@/shared/ui";

type PageHeaderProps = {
  title: ReactNode;
  fallback: string;
  backLabel?: string;
  children?: ReactNode;
};

function useScrolledFromTop(): boolean {
  const [scrolled, setScrolled] = useState(() => window.scrollY > 0);
  useEffect(() => {
    const update = () => {
      setScrolled(window.scrollY > 0);
    };
    update();
    window.addEventListener("scroll", update, { passive: true });
    return () => {
      window.removeEventListener("scroll", update);
    };
  }, []);
  return scrolled;
}

export function PageHeader({
  title,
  fallback,
  backLabel = "Go back",
  children,
}: PageHeaderProps) {
  const scrolled = useScrolledFromTop();
  return (
    <header
      data-vt-name="app-header"
      data-scrolled={scrolled ? "" : undefined}
      className="group sticky top-0 z-100"
    >
      {/* WHY: height-reserving strip mirrors AppHeader so PageHeader content
          sits at the same vertical center across all pages — without it, the
          notch absorbs single-row padding and pulls content ~10px higher. */}
      <div
        aria-hidden
        className="h-[env(safe-area-inset-top)] bg-background/0 transition-colors duration-200 group-data-scrolled:bg-background"
      />
      <div className="flex h-14 items-center gap-2 bg-background/0 ps-1 px-3 transition-colors duration-200 group-data-scrolled:bg-background">
        <BackButton fallback={fallback} aria-label={backLabel} />
        <h1 className="flex-1 truncate text-base font-medium">{title}</h1>
        {children}
      </div>
    </header>
  );
}

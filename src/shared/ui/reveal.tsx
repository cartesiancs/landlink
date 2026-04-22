import { type ReactNode } from "react";

import { cn, useInView } from "@/shared/lib";

type RevealProps = {
  children: ReactNode;
  delay?: number;
  threshold?: number;
  className?: string;
};

export function Reveal({
  children,
  delay = 0,
  threshold = 0.3,
  className,
}: RevealProps) {
  const [ref, inView] = useInView<HTMLDivElement>(threshold);
  return (
    <div
      ref={ref}
      style={{ transitionDelay: `${String(delay)}ms` }}
      className={cn(
        "transition-[opacity,transform] duration-1200 ease-out",
        inView ? "translate-y-0 opacity-100" : "translate-y-8 opacity-0",
        className,
      )}
    >
      {children}
    </div>
  );
}

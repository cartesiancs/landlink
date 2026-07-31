import {
  useEffect,
  useState,
  type CSSProperties,
  type ReactNode,
} from "react";
import { springCurve } from "@/shared/lib";

const SPRING = springCurve({ stiffness: 320, damping: 26 });
const STAGGER_MS = 60;

const HIDDEN_STYLE: CSSProperties = {
  opacity: 0,
  transform: "translateY(12px)",
  filter: "blur(8px)",
  willChange: "opacity, transform, filter",
};

export function Reveal({
  order,
  className,
  children,
}: {
  order: number;
  className?: string;
  children: ReactNode;
}) {
  const [shown, setShown] = useState(
    () => window.matchMedia("(prefers-reduced-motion: reduce)").matches,
  );

  useEffect(() => {
    if (shown) return;
    // Double rAF so the hidden state is painted before the transition starts.
    let inner = 0;
    const outer = requestAnimationFrame(() => {
      inner = requestAnimationFrame(() => {
        setShown(true);
      });
    });
    return () => {
      cancelAnimationFrame(outer);
      cancelAnimationFrame(inner);
    };
  }, [shown]);

  const delayMs = order * STAGGER_MS;
  const style: CSSProperties = shown
    ? {
        opacity: 1,
        transform: "translateY(0px)",
        filter: "blur(0px)",
        transition: [
          `opacity ${SPRING.durationMs}ms ${SPRING.clampedEasing} ${delayMs}ms`,
          `filter ${SPRING.durationMs}ms ${SPRING.clampedEasing} ${delayMs}ms`,
          `transform ${SPRING.durationMs}ms ${SPRING.easing} ${delayMs}ms`,
        ].join(", "),
      }
    : HIDDEN_STYLE;

  return (
    <div className={className} style={style}>
      {children}
    </div>
  );
}

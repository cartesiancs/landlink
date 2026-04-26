import { useEffect, useLayoutEffect, type RefObject } from "react";

const scrollPositions = new Map<string, number>();

export function useScrollRestoration(
  key: string,
  ref: RefObject<HTMLElement | null>,
): void {
  useLayoutEffect(() => {
    const el = ref.current;
    if (!el) return;
    const saved = scrollPositions.get(key);
    if (saved !== undefined) {
      el.scrollTop = saved;
    }
  }, [key, ref]);

  useEffect(() => {
    const el = ref.current;
    if (!el) return;
    const handleScroll = () => {
      scrollPositions.set(key, el.scrollTop);
    };
    el.addEventListener("scroll", handleScroll, { passive: true });
    return () => {
      el.removeEventListener("scroll", handleScroll);
    };
  }, [key, ref]);
}

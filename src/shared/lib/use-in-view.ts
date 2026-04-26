import { useEffect, useRef, useState } from "react";

type UseInViewOptions = {
  threshold?: number;
  once?: boolean;
  initialInView?: boolean;
};

export function useInView<T extends Element = HTMLElement>(
  options: UseInViewOptions = {},
) {
  const { threshold = 0.3, once = false, initialInView = false } = options;
  const ref = useRef<T | null>(null);
  const [inView, setInView] = useState(initialInView);

  useEffect(() => {
    const el = ref.current;
    if (!el) return;
    const observer = new IntersectionObserver(
      (entries) => {
        const entry = entries[0];
        if (!entry) return;
        if (entry.isIntersecting) {
          setInView(true);
          if (once) observer.disconnect();
        } else if (!once) {
          setInView(false);
        }
      },
      { threshold },
    );
    observer.observe(el);
    return () => {
      observer.disconnect();
    };
  }, [threshold, once]);

  return [ref, inView] as const;
}

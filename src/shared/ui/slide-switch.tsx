import {
  useEffect,
  useRef,
  useState,
  useSyncExternalStore,
  type ReactNode,
} from "react";

import { cn } from "@/shared/lib";

type SlideSwitchProps = {
  contentKey: string;
  children: ReactNode;
  className?: string;
  duration?: number;
  gap?: number;
};

type EntryStatus = "live" | "exiting";

type Entry = {
  key: string;
  content: ReactNode;
  status: EntryStatus;
};

function subscribeReducedMotion(onChange: () => void): () => void {
  const mql = window.matchMedia("(prefers-reduced-motion: reduce)");
  mql.addEventListener("change", onChange);
  return () => {
    mql.removeEventListener("change", onChange);
  };
}

function readReducedMotion(): boolean {
  return window.matchMedia("(prefers-reduced-motion: reduce)").matches;
}

function readReducedMotionServer(): boolean {
  return false;
}

function useReducedMotion(): boolean {
  return useSyncExternalStore(
    subscribeReducedMotion,
    readReducedMotion,
    readReducedMotionServer,
  );
}

export function SlideSwitch({
  contentKey,
  children,
  className,
  duration = 500,
  gap = 150,
}: SlideSwitchProps) {
  const reduced = useReducedMotion();
  const [entries, setEntries] = useState<Entry[]>(() => [
    { key: contentKey, content: children, status: "live" },
  ]);
  const [hasTransitioned, setHasTransitioned] = useState(false);
  // WHY: live entry always renders the latest `children` (so mutating props on
  // the body re-render normally); only exiting entries need a frozen snapshot
  // so the outgoing animation doesn't visually morph mid-flight. This ref
  // holds the previous render's children to feed that snapshot.
  const previousChildrenRef = useRef<ReactNode>(children);

  const liveKey = entries.find((entry) => entry.status === "live")?.key;

  if (liveKey !== contentKey) {
    const snapshot = previousChildrenRef.current;
    setEntries((prev) => {
      if (reduced) {
        return [{ key: contentKey, content: children, status: "live" }];
      }
      const withoutDuplicateIncoming = prev.filter(
        (entry) => entry.key !== contentKey,
      );
      return [
        ...withoutDuplicateIncoming.map((entry) =>
          entry.status === "live"
            ? { ...entry, status: "exiting" as const, content: snapshot }
            : entry,
        ),
        { key: contentKey, content: children, status: "live" as const },
      ];
    });
    setHasTransitioned(true);
  }

  useEffect(() => {
    previousChildrenRef.current = children;
  });

  const phaseMs = duration / 2;
  const shouldAnimateIn = hasTransitioned && !reduced;

  const handleExitEnd = (exitedKey: string) => {
    setEntries((prev) =>
      prev.filter(
        (entry) => !(entry.key === exitedKey && entry.status === "exiting"),
      ),
    );
  };

  return (
    <div
      className={cn(
        "relative isolate grid grid-cols-1 grid-rows-1 overflow-hidden",
        className,
      )}
    >
      {entries.map((entry) => {
        const isExiting = entry.status === "exiting";
        const isLive = entry.status === "live";

        const style = isExiting
          ? { animationDuration: `${String(phaseMs)}ms` }
          : isLive && shouldAnimateIn
          ? {
              animationDuration: `${String(phaseMs)}ms`,
              animationDelay: `${String(phaseMs + gap)}ms`,
            }
          : undefined;

        return (
          <div
            key={entry.key}
            aria-hidden={isExiting}
            style={style}
            className={cn(
              "col-start-1 row-start-1 min-h-0",
              isExiting &&
                "ease-[cubic-bezier(0.32,0.72,0,1)] animate-out fade-out slide-out-to-left-8 fill-mode-forwards",
              isLive &&
                shouldAnimateIn &&
                "ease-[cubic-bezier(0.32,0.72,0,1)] animate-in fade-in slide-in-from-right-8 fill-mode-both",
            )}
            onAnimationEnd={
              isExiting
                ? () => {
                    handleExitEnd(entry.key);
                  }
                : undefined
            }
          >
            {isLive ? children : entry.content}
          </div>
        );
      })}
    </div>
  );
}

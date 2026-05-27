import { useEffect } from "react";

import { Capacitor } from "@capacitor/core";
import { Keyboard } from "@capacitor/keyboard";

const VAR_NAME = "--keyboard-inset";

type Handle = { remove: () => Promise<void> };

export function useKeyboardInset(): void {
  useEffect(() => {
    if (!Capacitor.isNativePlatform()) return;

    const root = document.documentElement;
    const handles: Handle[] = [];
    let cancelled = false;

    function track(pending: Promise<Handle>): void {
      void pending.then((handle) => {
        if (cancelled) {
          void handle.remove();
          return;
        }
        handles.push(handle);
      });
    }

    track(
      Keyboard.addListener("keyboardWillShow", (info) => {
        root.style.setProperty(
          VAR_NAME,
          `${info.keyboardHeight.toString()}px`,
        );
      }),
    );
    track(
      Keyboard.addListener("keyboardWillHide", () => {
        root.style.setProperty(VAR_NAME, "0px");
      }),
    );

    return () => {
      cancelled = true;
      root.style.setProperty(VAR_NAME, "0px");
      for (const h of handles) {
        void h.remove();
      }
    };
  }, []);
}

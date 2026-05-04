import type { Theme } from "../model/types";

const THEME_COLOR_LIGHT = "#ffffff";
const THEME_COLOR_DARK = "#000000";

export function applyTheme(theme: Theme): void {
  if (typeof document === "undefined") return;

  const root = document.documentElement;
  if (theme === "dark") {
    root.classList.add("dark");
  } else {
    root.classList.remove("dark");
  }
  root.style.colorScheme = theme;

  const color = theme === "dark" ? THEME_COLOR_DARK : THEME_COLOR_LIGHT;
  const metas = document.querySelectorAll<HTMLMetaElement>(
    'meta[name="theme-color"]',
  );
  metas.forEach((meta) => {
    meta.content = color;
  });
}

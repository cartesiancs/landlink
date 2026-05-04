import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { MemoryRouter, Route, Routes } from "react-router-dom";

import { ROUTES } from "@/shared/config";

import { SettingsPage } from "./settings-page";

function renderWithRoutes() {
  return render(
    <MemoryRouter initialEntries={[ROUTES.settings]}>
      <Routes>
        <Route path={ROUTES.settings} element={<SettingsPage />} />
        <Route
          path={ROUTES.settingsTheme}
          element={<div data-testid="theme-page" />}
        />
        <Route
          path={ROUTES.settingsDebug}
          element={<div data-testid="debug-page" />}
        />
        <Route
          path={ROUTES.settingsReset}
          element={<div data-testid="reset-page" />}
        />
      </Routes>
    </MemoryRouter>,
  );
}

describe("SettingsPage (list view)", () => {
  it("renders one row per settings entry", () => {
    renderWithRoutes();
    expect(screen.getByRole("link", { name: /theme/i })).toBeInTheDocument();
    expect(
      screen.getByRole("link", { name: /debug mode/i }),
    ).toBeInTheDocument();
    expect(
      screen.getByRole("link", { name: /reset all data/i }),
    ).toBeInTheDocument();
  });

  it("Theme row navigates to /settings/theme", async () => {
    renderWithRoutes();
    await userEvent.click(screen.getByRole("link", { name: /theme/i }));
    expect(screen.getByTestId("theme-page")).toBeInTheDocument();
  });

  it("Debug mode row navigates to /settings/debug", async () => {
    renderWithRoutes();
    await userEvent.click(screen.getByRole("link", { name: /debug mode/i }));
    expect(screen.getByTestId("debug-page")).toBeInTheDocument();
  });

  it("Reset all data row navigates to /settings/reset", async () => {
    renderWithRoutes();
    await userEvent.click(
      screen.getByRole("link", { name: /reset all data/i }),
    );
    expect(screen.getByTestId("reset-page")).toBeInTheDocument();
  });
});

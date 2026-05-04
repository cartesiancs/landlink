import { render, screen } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";

vi.mock("@/features/toggle-theme", () => ({
  ThemeToggle: () => <div data-testid="theme-toggle" />,
}));

import { SettingsThemePage } from "./settings-theme-page";

function renderPage() {
  return render(
    <MemoryRouter>
      <SettingsThemePage />
    </MemoryRouter>,
  );
}

describe("SettingsThemePage", () => {
  it("renders the theme toggle", () => {
    renderPage();
    expect(screen.getByTestId("theme-toggle")).toBeInTheDocument();
  });

  it("renders a back link to /settings", () => {
    renderPage();
    const back = screen.getByRole("link", { name: /back to settings/i });
    expect(back).toHaveAttribute("href", "/settings");
  });
});

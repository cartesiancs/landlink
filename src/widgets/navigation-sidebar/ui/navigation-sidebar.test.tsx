import { render, screen } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";

import {
  _resetRegisteredDevicesStore,
  registerDevice,
} from "@/entities/registered-device";

vi.mock("@/features/toggle-theme", () => ({
  ThemeToggle: () => <div data-testid="theme-toggle" />,
}));

import { NavigationSidebar } from "./navigation-sidebar";

function renderSidebar() {
  return render(
    <MemoryRouter>
      <NavigationSidebar
        open
        onOpenChange={() => {
          /* noop */
        }}
      />
    </MemoryRouter>,
  );
}

describe("NavigationSidebar Lists visibility", () => {
  beforeEach(() => {
    _resetRegisteredDevicesStore();
  });

  it("hides 'Lists' when no devices are registered", () => {
    renderSidebar();
    expect(screen.queryByRole("link", { name: /lists/i })).not.toBeInTheDocument();
  });

  it("shows 'Lists' once a device has been registered", () => {
    registerDevice({ id: "a", name: "A", source: "ble" });
    renderSidebar();
    expect(screen.getByRole("link", { name: /lists/i })).toBeInTheDocument();
  });

  it("always shows other top-level items (Home, Settings, About)", () => {
    renderSidebar();
    expect(screen.getByRole("link", { name: /home/i })).toBeInTheDocument();
    expect(screen.getByRole("link", { name: /settings/i })).toBeInTheDocument();
    expect(screen.getByRole("link", { name: /about/i })).toBeInTheDocument();
  });
});

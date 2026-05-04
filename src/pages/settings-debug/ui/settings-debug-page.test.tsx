import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { MemoryRouter } from "react-router-dom";

import {
  _resetDebugModeStore,
  getDebugMode,
  setDebugMode,
} from "@/entities/debug-mode";
import {
  _resetRegisteredDevicesStore,
  getRegisteredDevices,
} from "@/entities/registered-device";

import { SettingsDebugPage } from "./settings-debug-page";

function renderPage() {
  return render(
    <MemoryRouter>
      <SettingsDebugPage />
    </MemoryRouter>,
  );
}

describe("SettingsDebugPage", () => {
  beforeEach(() => {
    _resetDebugModeStore();
    _resetRegisteredDevicesStore();
  });

  it("hides 'Register mock device' when debug mode is off", () => {
    renderPage();
    expect(
      screen.queryByRole("button", { name: /register mock device/i }),
    ).not.toBeInTheDocument();
  });

  it("toggling the switch persists debug mode and reveals the mock register button", async () => {
    renderPage();
    const debugSwitch = screen.getByRole("switch", { name: /debug mode/i });
    await userEvent.click(debugSwitch);
    expect(getDebugMode()).toBe(true);
    expect(
      screen.getByRole("button", { name: /register mock device/i }),
    ).toBeInTheDocument();
  });

  it("clicking 'Register mock device' adds a disabled mock to the registry", async () => {
    setDebugMode(true);
    renderPage();
    const button = screen.getByRole("button", { name: /register mock device/i });
    await userEvent.click(button);
    const list = getRegisteredDevices();
    expect(list).toHaveLength(1);
    expect(list[0]?.source).toBe("mock");
    expect(list[0]?.enabled).toBe(false);
  });

  it("renders a back link to /settings", () => {
    renderPage();
    const back = screen.getByRole("link", { name: /back to settings/i });
    expect(back).toHaveAttribute("href", "/settings");
  });
});

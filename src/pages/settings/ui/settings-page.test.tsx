import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { MemoryRouter } from "react-router-dom";

import {
  _resetRegisteredDevicesStore,
  getRegisteredDevices,
  registerDevice,
} from "@/entities/registered-device";
import {
  _resetDebugModeStore,
  getDebugMode,
  setDebugMode,
} from "@/entities/debug-mode";

vi.mock("@/entities/landlink-device", () => ({
  detachLandlinkClient: vi.fn(() => Promise.resolve()),
  useLandlinkDevice: () => null,
}));

vi.mock("@/features/toggle-theme", () => ({
  ThemeToggle: () => <div data-testid="theme-toggle" />,
}));

import { SettingsPage } from "./settings-page";

function renderSettings() {
  return render(
    <MemoryRouter>
      <SettingsPage />
    </MemoryRouter>,
  );
}

describe("SettingsPage", () => {
  beforeEach(() => {
    _resetRegisteredDevicesStore();
    _resetDebugModeStore();
  });

  it("hides 'Register mock device' when debug mode is off", () => {
    renderSettings();
    expect(
      screen.queryByRole("button", { name: /register mock device/i }),
    ).not.toBeInTheDocument();
  });

  it("shows 'Register mock device' when debug mode is on", async () => {
    renderSettings();
    const debugSwitch = screen.getByRole("switch", { name: /debug mode/i });
    await userEvent.click(debugSwitch);
    expect(getDebugMode()).toBe(true);
    expect(
      screen.getByRole("button", { name: /register mock device/i }),
    ).toBeInTheDocument();
  });

  it("clicking 'Register mock device' adds a disabled mock device to the registry", async () => {
    setDebugMode(true);
    renderSettings();
    const button = screen.getByRole("button", { name: /register mock device/i });
    await userEvent.click(button);
    const list = getRegisteredDevices();
    expect(list).toHaveLength(1);
    expect(list[0]?.source).toBe("mock");
    expect(list[0]?.enabled).toBe(false);
  });

  it("'Reset all data' opens confirm and clears registry + debug flag on confirm", async () => {
    setDebugMode(true);
    registerDevice({ id: "a", name: "A", source: "ble" });
    renderSettings();
    const reset = screen.getByRole("button", { name: /reset all data/i });
    await userEvent.click(reset);
    const confirm = await screen.findByRole("button", {
      name: /reset everything/i,
    });
    await userEvent.click(confirm);
    expect(getRegisteredDevices()).toEqual([]);
    expect(getDebugMode()).toBe(false);
  });

  it("'Reset all data' cancel preserves state", async () => {
    setDebugMode(true);
    registerDevice({ id: "a", name: "A", source: "ble" });
    renderSettings();
    const reset = screen.getByRole("button", { name: /reset all data/i });
    await userEvent.click(reset);
    const cancel = await screen.findByRole("button", { name: /cancel/i });
    await userEvent.click(cancel);
    expect(getRegisteredDevices()).toHaveLength(1);
    expect(getDebugMode()).toBe(true);
  });
});

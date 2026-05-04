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
  registerDevice,
} from "@/entities/registered-device";

vi.mock("@/entities/landlink-device", () => ({
  detachLandlinkClient: vi.fn(() => Promise.resolve()),
  useLandlinkDevice: () => null,
}));

import { SettingsResetPage } from "./settings-reset-page";

function renderPage() {
  return render(
    <MemoryRouter>
      <SettingsResetPage />
    </MemoryRouter>,
  );
}

describe("SettingsResetPage", () => {
  beforeEach(() => {
    _resetRegisteredDevicesStore();
    _resetDebugModeStore();
  });

  it("confirm clears registry and debug flag", async () => {
    setDebugMode(true);
    registerDevice({ id: "a", name: "A", source: "ble" });
    renderPage();
    await userEvent.click(screen.getByRole("button", { name: /reset all data/i }));
    const confirm = await screen.findByRole("button", {
      name: /reset everything/i,
    });
    await userEvent.click(confirm);
    expect(getRegisteredDevices()).toEqual([]);
    expect(getDebugMode()).toBe(false);
  });

  it("cancel preserves state", async () => {
    setDebugMode(true);
    registerDevice({ id: "a", name: "A", source: "ble" });
    renderPage();
    await userEvent.click(screen.getByRole("button", { name: /reset all data/i }));
    const cancel = await screen.findByRole("button", { name: /cancel/i });
    await userEvent.click(cancel);
    expect(getRegisteredDevices()).toHaveLength(1);
    expect(getDebugMode()).toBe(true);
  });

  it("renders a back link to /settings", () => {
    renderPage();
    const back = screen.getByRole("link", { name: /back to settings/i });
    expect(back).toHaveAttribute("href", "/settings");
  });
});

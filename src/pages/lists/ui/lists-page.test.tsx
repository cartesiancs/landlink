import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { MemoryRouter, Route, Routes } from "react-router-dom";

import {
  _resetRegisteredDevicesStore,
  addRegisteredDevice,
  registerDevice,
} from "@/entities/registered-device";
import { ROUTES } from "@/shared/config";

import { ListsPage } from "./lists-page";

function renderListsPage() {
  return render(
    <MemoryRouter initialEntries={[ROUTES.lists]}>
      <Routes>
        <Route path={ROUTES.lists} element={<ListsPage />} />
        <Route
          path={ROUTES.connectBluetooth}
          element={<div data-testid="connect-bluetooth-page">connect</div>}
        />
      </Routes>
    </MemoryRouter>,
  );
}

describe("ListsPage", () => {
  beforeEach(() => {
    _resetRegisteredDevicesStore();
  });

  it("renders empty state when no devices registered", () => {
    renderListsPage();
    expect(screen.getByText(/no devices yet/i)).toBeInTheDocument();
  });

  it("renders one row per registered device", () => {
    registerDevice({ id: "a", name: "Device A", source: "ble" });
    registerDevice({ id: "b", name: "Device B", source: "ble" });
    renderListsPage();
    expect(screen.getByText("Device A")).toBeInTheDocument();
    expect(screen.getByText("Device B")).toBeInTheDocument();
  });

  it("shows Mock badge for mock devices", () => {
    registerDevice({ id: "m", name: "Pretend", source: "mock" });
    renderListsPage();
    expect(screen.getByText("Pretend")).toBeInTheDocument();
    expect(screen.getByText(/^Mock$/)).toBeInTheDocument();
  });

  it("renders ping/last-connected as em-dash when null", () => {
    addRegisteredDevice({
      id: "z",
      name: "Empty",
      source: "ble",
      enabled: true,
      status: "disconnected",
      pingMs: null,
      signalDbm: null,
      lastConnectedAt: null,
      registeredAt: 1,
      nodeId: null,
    });
    renderListsPage();
    expect(screen.getByText(/— · Never/)).toBeInTheDocument();
  });

  it("remove button removes the device from the registry", async () => {
    registerDevice({ id: "a", name: "Device A", source: "ble" });
    renderListsPage();
    const remove = screen.getByRole("button", { name: /remove device a/i });
    await userEvent.click(remove);
    expect(screen.queryByText("Device A")).not.toBeInTheDocument();
    expect(screen.getByText(/no devices yet/i)).toBeInTheDocument();
  });

  it("'Connect new device' button navigates to /connect/bluetooth", async () => {
    renderListsPage();
    const button = screen.getByRole("button", { name: /connect new device/i });
    await userEvent.click(button);
    expect(screen.getByTestId("connect-bluetooth-page")).toBeInTheDocument();
  });
});

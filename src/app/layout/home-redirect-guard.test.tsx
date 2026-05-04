import { render, screen } from "@testing-library/react";
import { MemoryRouter, Route, Routes } from "react-router-dom";

import {
  _resetRegisteredDevicesStore,
  registerDevice,
} from "@/entities/registered-device";
import { ROUTES } from "@/shared/config";

vi.mock("@/pages/home", () => ({
  HomePage: () => <div data-testid="home-page" />,
}));

import { HomeOrListsRedirect } from "./home-redirect-guard";

function renderAt(initialEntries: { pathname: string; state?: unknown }[]) {
  return render(
    <MemoryRouter initialEntries={initialEntries}>
      <Routes>
        <Route path={ROUTES.home} element={<HomeOrListsRedirect />} />
        <Route
          path={ROUTES.lists}
          element={<div data-testid="lists-page" />}
        />
      </Routes>
    </MemoryRouter>,
  );
}

describe("HomeOrListsRedirect", () => {
  beforeEach(() => {
    _resetRegisteredDevicesStore();
  });

  it("renders Home when registry is empty", () => {
    renderAt([{ pathname: ROUTES.home }]);
    expect(screen.getByTestId("home-page")).toBeInTheDocument();
  });

  it("redirects to /lists when devices exist and no fromNav state", () => {
    registerDevice({ id: "a", name: "A", source: "ble" });
    renderAt([{ pathname: ROUTES.home }]);
    expect(screen.getByTestId("lists-page")).toBeInTheDocument();
    expect(screen.queryByTestId("home-page")).not.toBeInTheDocument();
  });

  it("renders Home when devices exist but fromNav is true", () => {
    registerDevice({ id: "a", name: "A", source: "ble" });
    renderAt([{ pathname: ROUTES.home, state: { fromNav: true } }]);
    expect(screen.getByTestId("home-page")).toBeInTheDocument();
    expect(screen.queryByTestId("lists-page")).not.toBeInTheDocument();
  });

  it("ignores unrelated state values", () => {
    registerDevice({ id: "a", name: "A", source: "ble" });
    renderAt([{ pathname: ROUTES.home, state: { fromNav: false } }]);
    expect(screen.getByTestId("lists-page")).toBeInTheDocument();
  });

  it("ignores non-object state", () => {
    registerDevice({ id: "a", name: "A", source: "ble" });
    renderAt([{ pathname: ROUTES.home, state: "string-state" }]);
    expect(screen.getByTestId("lists-page")).toBeInTheDocument();
  });
});

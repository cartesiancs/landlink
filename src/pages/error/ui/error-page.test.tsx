import { render, screen } from "@testing-library/react";
import {
  createMemoryRouter,
  RouterProvider,
  type RouteObject,
} from "react-router-dom";

import { ErrorPage } from "./error-page";

function ThrowsError({ thrown }: { thrown: unknown }): never {
  throw thrown;
}

function renderWithError(thrown: unknown) {
  const routes: RouteObject[] = [
    {
      errorElement: <ErrorPage />,
      children: [
        { path: "/", element: <ThrowsError thrown={thrown} /> },
        { path: "/home", element: <div data-testid="home" /> },
      ],
    },
  ];
  const router = createMemoryRouter(routes, { initialEntries: ["/"] });
  return render(<RouterProvider router={router} />);
}

describe("ErrorPage", () => {
  // WHY: react-router prints any thrown error to console.error via its
  // default error boundary plumbing. Silence to keep test output focused.
  beforeEach(() => {
    vi.spyOn(console, "error").mockImplementation(() => undefined);
  });

  it("catches a thrown Error and shows its message", () => {
    renderWithError(new Error("Device permission lost"));
    expect(screen.getByText("Something went wrong")).toBeInTheDocument();
    expect(screen.getByText("Device permission lost")).toBeInTheDocument();
    expect(screen.getByRole("button", { name: /reload/i })).toBeInTheDocument();
    expect(
      screen.getByRole("link", { name: /back to home/i }),
    ).toBeInTheDocument();
  });

  it("falls back to the error name when message is empty", () => {
    const err = new Error("");
    err.name = "BoundaryFault";
    renderWithError(err);
    expect(screen.getByText("BoundaryFault")).toBeInTheDocument();
  });

  it("renders a fallback title for non-Error throw values", () => {
    renderWithError("plain string");
    expect(screen.getByText("Something went wrong")).toBeInTheDocument();
    expect(screen.queryByText("plain string")).not.toBeInTheDocument();
  });

  it("Back to Home points at the home route", () => {
    renderWithError(new Error("boom"));
    const link = screen.getByRole("link", { name: /back to home/i });
    expect(link).toHaveAttribute("href", "/");
  });
});

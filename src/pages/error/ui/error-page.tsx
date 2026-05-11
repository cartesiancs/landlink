import { isRouteErrorResponse, Link, useRouteError } from "react-router-dom";

import { ROUTES } from "@/shared/config";
import { Button } from "@/shared/ui";

type Described = {
  title: string;
  detail: string | null;
};

function describeError(error: unknown): Described {
  if (isRouteErrorResponse(error)) {
    return {
      title: `${error.status.toString()} ${error.statusText}`,
      detail: typeof error.data === "string" ? error.data : null,
    };
  }
  if (error instanceof Error) {
    return {
      title: "Something went wrong",
      detail: error.message.length > 0 ? error.message : error.name,
    };
  }
  return { title: "Something went wrong", detail: null };
}

export function ErrorPage() {
  const error = useRouteError();
  const { title, detail } = describeError(error);

  const handleReload = (): void => {
    window.location.reload();
  };

  return (
    <main className="flex min-h-dvh flex-col items-center justify-center gap-4 px-6 text-center">
      <h1 className="text-2xl font-semibold">{title}</h1>
      {detail !== null && (
        <p className="max-w-md break-words text-sm text-muted-foreground">
          {detail}
        </p>
      )}
      <div className="flex gap-2">
        <Button onClick={handleReload}>Reload</Button>
        <Button variant="ghost" asChild>
          <Link to={ROUTES.home} viewTransition>
            Back to Home
          </Link>
        </Button>
      </div>
    </main>
  );
}

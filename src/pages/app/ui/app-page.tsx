import { useEffect } from "react";

import { APP_STORE_URL, ROUTES } from "@/shared/config";
import { detectIOS } from "@/shared/lib";
import { PageHeader } from "@/widgets/page-header";

export function AppPage() {
  const isIOS = detectIOS();

  useEffect(() => {
    if (isIOS) {
      window.location.replace(APP_STORE_URL);
    }
  }, [isIOS]);

  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <PageHeader
        title="Get the App"
        fallback={ROUTES.home}
        backLabel="Back to Home"
      />

      <section className="px-4 pt-2 pb-6">
        {isIOS ? (
          <>
            <h2 className="font-display text-3xl leading-tight tracking-tight">
              Opening the App Store
            </h2>
            <p className="mt-3 text-sm leading-relaxed text-muted-foreground">
              Hang tight, redirecting you to Landlink on iOS.
            </p>
          </>
        ) : (
          <>
            <h2 className="font-display text-3xl leading-tight tracking-tight">
              Coming soon
            </h2>
            <p className="mt-3 text-sm leading-relaxed text-muted-foreground">
              Landlink is launching first on iOS. Android support is on the way.
              We will let you know the moment it lands.
            </p>
          </>
        )}
      </section>
    </main>
  );
}

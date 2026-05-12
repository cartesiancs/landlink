import { Capacitor } from "@capacitor/core";
import { useEffect, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { useLocation, useNavigate } from "react-router-dom";

import { ROUTES } from "@/shared/config";
import { cn, hapticTick, useInView, useScrollRestoration } from "@/shared/lib";
import { Button } from "@/shared/ui";
import { BottomNavBar } from "@/widgets/bottom-nav-bar";
import { HomeCommunity } from "@/widgets/home-community";
import { HomeHeroCarousel } from "@/widgets/home-hero-carousel";
import { HomeLinkList } from "@/widgets/home-link-list";
import { HomeStep } from "@/widgets/home-step";

// WHY: portal the scroll surface to <body> with fixed bounds matching the
// AppLayout chrome. This lets the snap-scroll viewport span the full window on
// PC (so the gray side strips capture wheel events natively) without breaking
// the 430px column used by the rest of the app.
const HOME_MAIN_TOP = "calc(env(safe-area-inset-top) + 3.5rem)";
const HOME_MAIN_BOTTOM = Capacitor.isNativePlatform()
  ? "calc(4.5rem + max(env(safe-area-inset-bottom), 0.75rem))"
  : "calc(3.75rem + max(env(safe-area-inset-bottom), 0.75rem))";

function GroundStationMedia() {
  const [ref, inView] = useInView<HTMLDivElement>({
    threshold: 0.4,
    once: true,
  });
  return (
    <div
      ref={ref}
      className="flex h-full w-full items-center justify-center gap-2"
    >
      <img
        src="/images/groundstation.webp"
        alt=""
        aria-hidden
        className={cn(
          "h-[55%] w-auto max-w-[26%] object-contain transition-opacity duration-500",
          inView ? "opacity-55" : "opacity-0",
        )}
      />
      <img
        src="/images/groundstation.webp"
        alt="Ground station"
        className="h-[80%] w-auto max-w-[45%] object-contain"
      />
      <img
        src="/images/groundstation.webp"
        alt=""
        aria-hidden
        className={cn(
          "h-[55%] w-auto max-w-[26%] object-contain transition-opacity duration-500",
          inView ? "opacity-55" : "opacity-0",
        )}
      />
    </div>
  );
}

let lastVisibleIntent = false;

export function HomePage() {
  const [buyStepRef, buyStepInView] = useInView<HTMLElement>({
    threshold: 0.5,
    once: false,
    initialInView: lastVisibleIntent,
  });
  const mainRef = useRef<HTMLElement>(null);
  const [isIdle, setIsIdle] = useState(lastVisibleIntent);
  const navigate = useNavigate();
  const location = useLocation();
  const isLeaving = location.pathname !== ROUTES.home;

  useScrollRestoration("home", mainRef);

  useEffect(() => {
    const el = mainRef.current;
    if (!el) return;
    let idleTimer: number | null = null;

    const clearIdleTimer = () => {
      if (idleTimer !== null) {
        window.clearTimeout(idleTimer);
        idleTimer = null;
      }
    };

    const scheduleIdle = (delay: number) => {
      clearIdleTimer();
      idleTimer = window.setTimeout(() => {
        setIsIdle(true);
      }, delay);
    };

    const handleScroll = () => {
      setIsIdle(false);
      scheduleIdle(400);
    };

    const handleScrollEnd = () => {
      clearIdleTimer();
      setIsIdle(true);
    };

    scheduleIdle(600);

    el.addEventListener("scroll", handleScroll, { passive: true });
    el.addEventListener("scrollend", handleScrollEnd);
    return () => {
      el.removeEventListener("scroll", handleScroll);
      el.removeEventListener("scrollend", handleScrollEnd);
      clearIdleTimer();
    };
  }, []);

  const showPurchase = isIdle && buyStepInView && !isLeaving;
  const visibleIntent = isIdle && buyStepInView;

  useEffect(() => {
    lastVisibleIntent = visibleIntent;
  }, [visibleIntent]);

  return (
    <>
      {createPortal(
        <main
          ref={mainRef}
          className={cn(
            "fixed left-0 right-0 z-0 snap-y snap-mandatory overflow-y-auto overscroll-contain bg-background transition-opacity duration-300 ease-out [scrollbar-width:none] [&::-webkit-scrollbar]:hidden",
            isLeaving
              ? "pointer-events-none opacity-0"
              : "pointer-events-auto opacity-100",
          )}
          style={{ top: HOME_MAIN_TOP, bottom: HOME_MAIN_BOTTOM }}
          aria-hidden={isLeaving}
        >
          <section className="flex h-full shrink-0 snap-start snap-always flex-col overflow-hidden">
            <div className="mx-auto flex h-full w-full max-w-[430px] flex-col">
              <div className="shrink-0 px-4 pt-4 pb-3">
                <h1 className="font-display text-3xl font-normal leading-tight tracking-tight">
                  Alternatives <br /> to Starlink
                </h1>
              </div>

              <div className="shrink-0 px-4 pb-4">
                <HomeHeroCarousel />
              </div>

              <div className="shrink-0 px-4 pb-4">
                <HomeLinkList />
              </div>
            </div>
          </section>

          <section
            ref={buyStepRef}
            className="h-full shrink-0 snap-start snap-always overflow-hidden"
          >
            <div className="mx-auto h-full w-full max-w-[430px]">
              <HomeStep
                step={1}
                title="Buy a Drone"
                description="Powered by cartesiancs' technology, you can easily set up a drone with a single connection that reaches up to 50km in range."
                media={
                  <img
                    src="/images/drone.webp"
                    alt="Drone"
                    className={cn(
                      "max-h-[75%] max-w-[80%] animate-drone-fly object-contain will-change-transform",
                      !buyStepInView && "paused",
                    )}
                  />
                }
              />
            </div>
          </section>

          <section className="h-full shrink-0 snap-start snap-always overflow-hidden">
            <div className="mx-auto h-full w-full max-w-[430px]">
              <HomeStep
                step={2}
                title="Connect Your Drone"
                description="Get connected in under a minute. No hassle, no complicated setup."
                media={
                  <div className="relative flex h-full w-full items-center justify-center">
                    <img
                      src="/images/drone.webp"
                      alt="Drone"
                      className="max-h-[60%] max-w-[65%] translate-y-[-12%] object-contain"
                    />
                    <img
                      src="/images/hand.webp"
                      alt="Hand holding phone"
                      className="absolute top-[10%] right-[8%] max-h-[45%] max-w-[45%] origin-bottom-right animate-hand-tilt object-contain"
                    />
                  </div>
                }
              />
            </div>
          </section>

          <section className="h-full shrink-0 snap-start snap-always overflow-hidden">
            <div className="mx-auto h-full w-full max-w-[430px]">
              <HomeStep
                step={3}
                title="Ground Station Setup"
                description="Build a mesh network with simple, plug-and-play modules. The ground station ships ready to relay text payloads, and you can add camera, microphone, or speaker modules whenever you need richer media."
                media={<GroundStationMedia />}
              />
            </div>
          </section>

          <section className="h-full shrink-0 snap-start snap-always overflow-hidden">
            <div className="mx-auto h-full w-full max-w-[430px]">
              <HomeCommunity />
            </div>
          </section>
        </main>,
        document.body,
      )}

      {createPortal(
        <div
          className={cn(
            "pointer-events-none fixed inset-x-0 bottom-[calc(env(safe-area-inset-bottom,12px)+72px)] z-20 mx-auto max-w-[430px] px-4 transition-all duration-300 ease-out",
            showPurchase
              ? "translate-y-0 opacity-100"
              : "translate-y-[120%] opacity-0",
          )}
          aria-hidden={!showPurchase}
        >
          <Button
            variant="outline"
            className={cn(
              "h-12 w-full bg-background text-base",
              showPurchase ? "pointer-events-auto" : "pointer-events-none",
            )}
            onClick={() => {
              hapticTick();
              void navigate(ROUTES.landlinkOne, { viewTransition: true });
            }}
            tabIndex={showPurchase ? 0 : -1}
          >
            Purchase
          </Button>
        </div>,
        document.body,
      )}

      <BottomNavBar />
    </>
  );
}

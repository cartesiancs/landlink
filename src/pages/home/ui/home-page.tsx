import { useEffect, useRef, useState } from "react";
import { useNavigate } from "react-router-dom";

import { ROUTES } from "@/shared/config";
import { cn, hapticTick, useInView } from "@/shared/lib";
import { Button } from "@/shared/ui";
import { HomeCommunity } from "@/widgets/home-community";
import { HomeHeroCarousel } from "@/widgets/home-hero-carousel";
import { HomeLinkList } from "@/widgets/home-link-list";
import { HomeStep } from "@/widgets/home-step";

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

export function HomePage() {
  const [buyStepRef, buyStepInView] = useInView<HTMLElement>({
    threshold: 0.5,
    once: false,
  });
  const mainRef = useRef<HTMLElement>(null);
  const [isIdle, setIsIdle] = useState(false);
  const navigate = useNavigate();

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

  const showPurchase = isIdle && buyStepInView;

  return (
    <>
      <main
        ref={mainRef}
        className="relative h-full snap-y snap-mandatory overflow-y-auto overscroll-contain [scrollbar-width:none] [&::-webkit-scrollbar]:hidden"
      >
        <section className="flex h-full shrink-0 snap-start snap-always flex-col overflow-hidden">
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
        </section>

        <section
          ref={buyStepRef}
          className="h-full shrink-0 snap-start snap-always overflow-hidden"
        >
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
        </section>

        <section className="h-full shrink-0 snap-start snap-always overflow-hidden">
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
        </section>

        <section className="h-full shrink-0 snap-start snap-always overflow-hidden">
          <HomeStep
            step={3}
            title="Ground Station Setup"
            description="Build a Mesh network with simple, plug-and-play modules. Get started right away with a ground station that comes with a camera, microphone, speaker, and communication module built in."
            media={<GroundStationMedia />}
          />
        </section>

        <section className="h-full shrink-0 snap-start snap-always overflow-hidden">
          <HomeCommunity />
        </section>
      </main>

      <div
        className={cn(
          "pointer-events-none fixed inset-x-0 bottom-[calc(env(safe-area-inset-bottom,12px)+72px)] z-20 mx-auto max-w-[430px] px-4 transition-all duration-500 ease-out",
          showPurchase
            ? "translate-y-0 opacity-100"
            : "translate-y-[120%] opacity-0",
        )}
        aria-hidden={!showPurchase}
      >
        <Button
          variant="outline"
          className="pointer-events-auto h-12 w-full bg-background text-base"
          onClick={() => {
            hapticTick();
            void navigate(ROUTES.purchase, { viewTransition: true });
          }}
          tabIndex={showPurchase ? 0 : -1}
        >
          Purchase
        </Button>
      </div>
    </>
  );
}

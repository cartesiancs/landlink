import { useEffect, useState } from "react";

import { cn } from "@/shared/lib";
import {
  Carousel,
  CarouselContent,
  CarouselItem,
  CarouselNext,
  CarouselPrevious,
  type CarouselApi,
} from "@/shared/ui";

const SLIDE_TINTS = [
  "bg-muted",
  "bg-card",
  "bg-secondary",
  "bg-accent",
  "bg-muted",
] as const;

const SLIDE_LABELS = [
  "Front view",
  "Pairing with phone",
  "In hand",
  "On a desk",
  "Carry case",
] as const;

export function ProductImageCarousel() {
  const [api, setApi] = useState<CarouselApi>();
  const [current, setCurrent] = useState(0);
  const slideCount = SLIDE_TINTS.length;

  useEffect(() => {
    if (!api) return;
    const sync = () => {
      setCurrent(api.selectedScrollSnap());
    };
    sync();
    api.on("select", sync);
    api.on("reInit", sync);
    return () => {
      api.off("select", sync);
      api.off("reInit", sync);
    };
  }, [api]);

  return (
    <div className="flex flex-col gap-4 lg:gap-6">
      <Carousel
        setApi={setApi}
        opts={{ loop: true }}
        className="relative w-full"
      >
        <CarouselContent>
          {SLIDE_TINTS.map((tint, index) => (
            <CarouselItem key={`${tint}-${index.toString()}`}>
              <div
                className={cn(
                  "flex aspect-square w-full items-center justify-center overflow-hidden rounded-3xl border border-border lg:aspect-[4/5]",
                  tint,
                )}
              >
                <img
                  src="/images/moduleone.webp"
                  alt={`Landlink Module I, ${SLIDE_LABELS[index] ?? "view"}`}
                  className={cn(
                    "w-auto object-contain transition-transform",
                    index % 2 === 0 ? "h-[68%]" : "h-[54%]",
                  )}
                  draggable={false}
                />
              </div>
            </CarouselItem>
          ))}
        </CarouselContent>
        <CarouselPrevious className="left-3 lg:left-4" />
        <CarouselNext className="right-3 lg:right-4" />
      </Carousel>

      <div
        role="tablist"
        aria-label="Product image slides"
        className="flex items-center justify-center gap-2"
      >
        {Array.from({ length: slideCount }).map((_, index) => {
          const active = current === index;
          return (
            <button
              key={`dot-${index.toString()}`}
              type="button"
              role="tab"
              aria-selected={active}
              aria-label={`Show slide ${(index + 1).toString()}`}
              onClick={() => {
                api?.scrollTo(index);
              }}
              className={cn(
                "h-1.5 rounded-full transition-all",
                active
                  ? "w-6 bg-foreground"
                  : "w-1.5 bg-border hover:bg-foreground/40",
              )}
            />
          );
        })}
      </div>
    </div>
  );
}

import { useEffect, useState } from "react";

import {
  Carousel,
  CarouselContent,
  CarouselItem,
  type CarouselApi,
} from "@/shared/ui";

import starlinkDishSrc from "../assets/starlink-dish.webp";
import groundStationSrc from "../assets/ground-station.webp";
import constellationSrc from "../assets/constellation.webp";

const AUTOPLAY_INTERVAL_MS = 5000;

type Slide = {
  id: string;
  src: string;
  alt: string;
};

const SLIDES: readonly Slide[] = [
  {
    id: "starlink-dish",
    src: starlinkDishSrc,
    alt: "Satellite dish against a night sky",
  },
  {
    id: "ground-station",
    src: groundStationSrc,
    alt: "Earth from orbit with communication links",
  },
  {
    id: "constellation",
    src: constellationSrc,
    alt: "Low-earth-orbit satellite constellation rendering",
  },
];

export function HomeHeroCarousel() {
  const [api, setApi] = useState<CarouselApi>();

  useEffect(() => {
    if (!api) return;
    const id = window.setInterval(() => {
      api.scrollNext();
    }, AUTOPLAY_INTERVAL_MS);
    return () => {
      window.clearInterval(id);
    };
  }, [api]);

  return (
    <Carousel
      setApi={setApi}
      opts={{ loop: true, align: "start" }}
      className="w-full overflow-hidden rounded-2xl"
      aria-label="Featured alternatives"
    >
      <CarouselContent className="ml-0">
        {SLIDES.map((slide) => (
          <CarouselItem key={slide.id} className="pl-0 basis-full">
            <div className="relative aspect-[16/10] w-full bg-muted">
              <img
                src={slide.src}
                alt={slide.alt}
                className="h-full w-full object-cover"
                loading="lazy"
              />
            </div>
          </CarouselItem>
        ))}
      </CarouselContent>
    </Carousel>
  );
}

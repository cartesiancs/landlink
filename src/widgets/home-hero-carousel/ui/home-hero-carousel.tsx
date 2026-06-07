import { Capacitor } from "@capacitor/core";
import { useEffect, useRef, useState } from "react";

import {
  Carousel,
  CarouselContent,
  CarouselItem,
  type CarouselApi,
} from "@/shared/ui";
import { cn } from "@/shared/lib";

import bannerSrc from "../assets/banner.jpg";
import starlinkDishSrc from "../assets/starlink-dish.webp";
import groundStationSrc from "../assets/ground-station.webp";
import constellationSrc from "../assets/constellation.webp";

const AUTOPLAY_INTERVAL_MS = 5000;

type Slide = {
  id: string;
  src: string;
  alt: string;
};

// WHY: Apple App Store brand review flagged the Starlink assets. iOS gets a
// single brand-safe banner slide; web and Android keep the original three.
const IS_IOS_APP = Capacitor.getPlatform() === "ios";

const SLIDES: readonly Slide[] = IS_IOS_APP
  ? [
      {
        id: "banner",
        src: bannerSrc,
        alt: "Landlink connectivity banner",
      },
    ]
  : [
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

const loadedSources = new Set<string>();

if (typeof window !== "undefined") {
  for (const slide of SLIDES) {
    const img = new Image();
    img.decoding = "async";
    img.onload = () => {
      loadedSources.add(slide.src);
    };
    img.src = slide.src;
  }
}

type CarouselSlideProps = {
  slide: Slide;
  eager: boolean;
};

function CarouselSlide({ slide, eager }: CarouselSlideProps) {
  const [loaded, setLoaded] = useState(() => loadedSources.has(slide.src));
  const imgRef = useRef<HTMLImageElement>(null);

  useEffect(() => {
    if (loaded) return;
    const img = imgRef.current;
    if (img && img.complete && img.naturalWidth > 0) {
      loadedSources.add(slide.src);
      setLoaded(true);
    }
  }, [loaded, slide.src]);

  return (
    <div className="relative aspect-16/10 w-full bg-muted">
      <img
        ref={imgRef}
        src={slide.src}
        alt={slide.alt}
        className={cn(
          "h-full w-full object-cover transition-opacity duration-500 ease-out",
          loaded ? "opacity-100" : "opacity-0",
        )}
        loading={eager ? "eager" : "lazy"}
        decoding="async"
        fetchPriority={eager ? "high" : "auto"}
        onLoad={() => {
          loadedSources.add(slide.src);
          setLoaded(true);
        }}
      />
    </div>
  );
}

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
        {SLIDES.map((slide, index) => (
          <CarouselItem key={slide.id} className="pl-0 basis-full">
            <CarouselSlide slide={slide} eager={index === 0} />
          </CarouselItem>
        ))}
      </CarouselContent>
    </Carousel>
  );
}

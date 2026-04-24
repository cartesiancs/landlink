import { cn } from "@/shared/lib";
import { Reveal } from "@/shared/ui";

type Review = {
  id: string;
  name: string;
  content: string;
};

const ROW_ONE: readonly Review[] = [
  {
    id: "r1",
    name: "H. Jun Huh",
    content: "Clear signal even from the summit. Exceeded my expectations.",
  },
  {
    id: "r2",
    name: "Sam Carter",
    content: "Setup took under a minute. Honestly surprised it just worked.",
  },
  {
    id: "r3",
    name: "Minhyeok Park",
    content:
      "Text messages get through in rough terrain where nothing else will.",
  },
  {
    id: "r4",
    name: "Alex Mitchell",
    content: "The mesh extends way past what I thought possible.",
  },
  {
    id: "r5",
    name: "Yuna Oh",
    content: "Short messages land almost instantly. No lag worth mentioning.",
  },
];

const ROW_TWO: readonly Review[] = [
  {
    id: "r6",
    name: "Daniel Ryu",
    content: "Ground station is dead simple — plug in and go.",
  },
  {
    id: "r7",
    name: "Hana Campbell",
    content: "Rock solid along the coast. Easy recommendation.",
  },
  {
    id: "r8",
    name: "Leo Martin",
    content: "Replaced my previous rig entirely. No regrets.",
  },
  {
    id: "r9",
    name: "Jin Rivera",
    content:
      "Had no idea installation could be this easy. Total beginner here.",
  },
  {
    id: "r10",
    name: "Mark Davis",
    content: "Range is the real deal. Flew 40km out without issue.",
  },
];

function ReviewCard({ review }: { review: Review }) {
  return (
    <div className="flex h-28 w-64 shrink-0 flex-col rounded-2xl border border-border bg-background p-4">
      <p className="text-xs font-medium">{review.name}</p>
      <p className="mt-2 line-clamp-3 text-sm leading-relaxed text-muted-foreground">
        {review.content}
      </p>
    </div>
  );
}

type MarqueeDirection = "left" | "right";

function Marquee({
  items,
  direction,
}: {
  items: readonly Review[];
  direction: MarqueeDirection;
}) {
  return (
    <div
      className="relative w-full overflow-hidden"
      style={{
        maskImage:
          "linear-gradient(to right, transparent, black 8%, black 92%, transparent)",
        WebkitMaskImage:
          "linear-gradient(to right, transparent, black 8%, black 92%, transparent)",
      }}
    >
      <div
        className={cn(
          "flex w-max gap-3",
          direction === "left"
            ? "animate-marquee-left"
            : "animate-marquee-right",
        )}
        aria-hidden={false}
      >
        {[...items, ...items].map((review, i) => (
          <ReviewCard key={`${review.id}-${String(i)}`} review={review} />
        ))}
      </div>
    </div>
  );
}

export function HomeCommunity() {
  return (
    <div className="flex h-full flex-col justify-center px-4 pt-8 pb-4">
      <Reveal>
        <h2 className="text-center text-3xl font-normal leading-tight tracking-tight">
          Powerful community
        </h2>
      </Reveal>

      <Reveal className="mt-3" delay={100}>
        <p className="text-center text-sm leading-relaxed text-muted-foreground">
          Build your own communication infrastructure and stay connected over
          long distances.
        </p>
      </Reveal>

      <Reveal className="mt-10 flex flex-col gap-3" delay={200}>
        <Marquee items={ROW_ONE} direction="left" />
        <Marquee items={ROW_TWO} direction="right" />
      </Reveal>
    </div>
  );
}

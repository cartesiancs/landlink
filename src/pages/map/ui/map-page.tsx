import { BottomNavBar } from "@/widgets/bottom-nav-bar";
import { LandlinkMap } from "@/widgets/landlink-map";

export function MapPage() {
  return (
    <main className="relative h-dvh w-full bg-background">
      <LandlinkMap />
      <BottomNavBar />
    </main>
  );
}

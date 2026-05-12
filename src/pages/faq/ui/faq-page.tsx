import { ChevronDown } from "lucide-react";

import { ROUTES } from "@/shared/config";
import { PageHeader } from "@/widgets/page-header";

type FaqItem = {
  question: string;
  answer: string;
};

const FAQS: readonly FaqItem[] = [
  {
    question: "What is Landlink?",
    answer:
      "Landlink is a long-range text communication platform built on small radio modules and drones. Think of it like Meshtastic, but with airborne relays you can fly wherever coverage is needed. Every node ships ready to send and receive text payloads out of the box.",
  },
  {
    question: "How does Landlink work?",
    answer:
      "Landlink Modules form a low-power mesh with the modules around them. When you add a Landlink drone, it joins the same mesh as a flying node and stretches coverage up to 50km from the ground. The link is two-way, so the network that carries your messages can also keep the drones in formation.",
  },
  {
    question: "Can I control the drones from my phone?",
    answer:
      "Yes. Because the mesh is bidirectional, the same browser tab that pairs with your module can reposition any drone, set a new patrol path, or recall the fleet. There is no separate ground controller to carry around.",
  },
  {
    question: "What do I need to get started?",
    answer:
      "A Landlink Module I is enough to start a small mesh on its own. Add a Landlink I drone whenever you need to push coverage further. Each kit ships with the radio module pre-configured for text payloads, and camera, microphone, and speaker modules are sold separately for when you need richer media.",
  },
  {
    question: "How far can I reach with my own ground station?",
    answer:
      "A single drone reaches up to 50km from a ground node. You can stand up your own ground stations or chain additional drones to push the mesh further across a campsite, ridge line, or remote worksite.",
  },
  {
    question: "Do I need an internet connection to set up Landlink?",
    answer:
      "Only for the very first visit, so the browser can load the web app. Pairing happens locally over Web Bluetooth between your tab and the hardware, and the mesh itself runs entirely offline once it is up.",
  },
  {
    question: "How long does the drone stay in the air?",
    answer:
      "Flight time depends on the model and weather. Flight batteries are hot-swappable, so you can rotate fresh packs through the ground node without dropping the mesh.",
  },
  {
    question: "How do I get support?",
    answer:
      "Open the support drawer from the header and tap Email support. Our team typically responds within one business day.",
  },
];

export function FaqPage() {
  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <PageHeader
        title="FAQ"
        fallback={ROUTES.home}
        backLabel="Back to Home"
      />

      <section className="px-4 pt-2 pb-6">
        <h2 className="font-display text-3xl leading-tight tracking-tight">
          Frequently asked
          <br />
          questions
        </h2>
      </section>

      <section className="flex flex-col gap-2 px-4 pb-10">
        {FAQS.map((item) => (
          <details
            key={item.question}
            className="group rounded-lg border border-border bg-card px-4 py-3 open:bg-muted/40"
          >
            <summary className="flex cursor-pointer list-none items-center justify-between gap-3 text-sm font-medium">
              {item.question}
              <ChevronDown
                className="size-4 shrink-0 text-muted-foreground transition-transform duration-200 group-open:rotate-180"
                aria-hidden="true"
              />
            </summary>
            <p className="mt-3 text-sm leading-relaxed text-muted-foreground">
              {item.answer}
            </p>
          </details>
        ))}
      </section>

      <footer className="mt-auto border-t border-border px-4 pt-6 pb-[max(env(safe-area-inset-bottom),1.5rem)] text-xs text-muted-foreground">
        Still have questions?{" "}
        <a
          href="mailto:jun@cartesiancs.com"
          className="underline underline-offset-2 hover:text-foreground"
        >
          Contact support
        </a>
        .
      </footer>
    </main>
  );
}

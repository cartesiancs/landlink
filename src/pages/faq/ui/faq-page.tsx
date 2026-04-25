import { ChevronLeft, ChevronDown } from "lucide-react";
import { Link } from "react-router-dom";

import { ROUTES } from "@/shared/config";

type FaqItem = {
  question: string;
  answer: string;
};

const FAQS: readonly FaqItem[] = [
  {
    question: "What is Landlink?",
    answer:
      "Landlink is a drone-powered connectivity platform that provides high-speed internet access in areas where traditional infrastructure is unavailable or unreliable. It serves as a practical alternative to satellite services like Starlink.",
  },
  {
    question: "How does Landlink work?",
    answer:
      "Landlink combines autonomous drones with a ground station to build a mesh network. Drones act as airborne relays that extend coverage up to 50km, while the ground station aggregates the signal and distributes it to your devices.",
  },
  {
    question: "What do I need to get started?",
    answer:
      "You need a Landlink-compatible drone and a ground station kit. Both come with a camera, microphone, speaker, and communication module preinstalled. Setup takes less than a minute through the Landlink app.",
  },
  {
    question: "What is the coverage range?",
    answer:
      "A single drone reaches up to 50km from the ground station. Multiple drones can be chained in a mesh network to extend coverage further across your property, campsite, or remote worksite.",
  },
  {
    question: "Do I need an internet connection to set up Landlink?",
    answer:
      "You need a one-time internet connection to pair your drone and complete the initial setup. After pairing, Landlink operates independently over its own mesh network.",
  },
  {
    question: "How long does the drone stay in the air?",
    answer:
      "Flight time varies by model and weather conditions. Ground stations are designed to swap drones automatically so your network stays up while batteries are cycled.",
  },
  {
    question: "Is Landlink available in my region?",
    answer:
      "Landlink is rolling out region by region, subject to local aviation regulations. Contact our support team to check availability and compliance requirements in your area.",
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
      <header className="sticky top-0 z-10 flex items-center gap-2 bg-background/90 px-4 ps-1 pt-[max(env(safe-area-inset-top),0.75rem)] pb-3 backdrop-blur supports-backdrop-filter:bg-background/70">
        <Link
          to={ROUTES.home}
          className="flex size-9 items-center justify-center rounded-md hover:bg-muted"
          aria-label="Back to Home"
        >
          <ChevronLeft className="size-5" aria-hidden="true" />
        </Link>
        <h1 className="text-base font-medium">FAQ</h1>
      </header>

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

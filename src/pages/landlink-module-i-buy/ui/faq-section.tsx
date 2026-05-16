import { ChevronDown } from "lucide-react";

type FaqItem = {
  question: string;
  answer: string;
};

const FAQS: readonly FaqItem[] = [
  {
    question: "Which frequency band should I pick?",
    answer:
      "Choose 915 MHz if you live in North America, Australia, or parts of Asia. Pick 868 MHz for Europe, the Middle East, or Africa. Using the wrong band for your region may not be legal, so match the option to where you'll actually use the mesh.",
  },
  {
    question: "How many nodes do I really need?",
    answer:
      "A single node lets you experiment with the mesh, but you need at least two devices to send messages. The 2-Pack is the smallest practical kit. The 4-Pack works well for a small team or a small property. If you're planning a larger deployment, you can always add more nodes later.",
  },
  {
    question: "When will my order ship?",
    answer:
      "Orders typically ship within 3 to 5 business days. You'll get a tracking link by email as soon as your kit leaves our warehouse.",
  },
  {
    question: "Do I need an internet connection to use it?",
    answer:
      "Only for the very first setup, so the browser can load the web app. Pairing happens locally over Web Bluetooth, and the mesh itself runs entirely offline once it's up.",
  },
  {
    question: "What's covered by the warranty?",
    answer:
      "Every Module I includes a one-year limited warranty against manufacturing defects. Accidental damage from drops or water is not covered by the standard warranty.",
  },
  {
    question: "Can I return it if it's not for me?",
    answer:
      "Yes. You have 30 days from delivery to return your kit for a full refund, as long as the hardware is in original condition. Reach out to support to start a return.",
  },
];

export function FaqSection() {
  return (
    <section
      aria-labelledby="buy-faq-title"
      className="mx-auto w-full max-w-7xl px-4 pt-4 pb-16 lg:px-8 lg:pt-12 lg:pb-24"
    >
      <header className="pt-10 lg:pt-14">
        <h2
          id="buy-faq-title"
          className="mt-2 font-display text-3xl leading-tight tracking-tight lg:text-4xl"
        >
          Frequently Asked Questions
        </h2>
      </header>

      <div className="mt-8 flex flex-col gap-2 lg:mt-10">
        {FAQS.map((item) => (
          <details
            key={item.question}
            className="group rounded-lg border border-border bg-card px-4 py-3 open:bg-muted/40 lg:px-5 lg:py-4"
          >
            <summary className="flex cursor-pointer list-none items-center justify-between gap-3 text-sm font-medium lg:text-base">
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
      </div>
    </section>
  );
}

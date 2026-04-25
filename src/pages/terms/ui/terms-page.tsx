import { ChevronLeft } from "lucide-react";
import { Link } from "react-router-dom";

import { ROUTES } from "@/shared/config";

type Section = {
  title: string;
  paragraphs: readonly string[];
};

const LAST_UPDATED = "April 24, 2026";

const SECTIONS: readonly Section[] = [
  {
    title: "Agreement",
    paragraphs: [
      "These terms govern your use of the Landlink website at landlink.sh and the Landlink web app used to pair, configure, and update Landlink hardware. By using the site, you agree to these terms. If you do not agree, please stop using the site.",
    ],
  },
  {
    title: "Who we are",
    paragraphs: [
      'Landlink is a project operated by cartesiancs ("we", "us"). References to "you" mean the person using the site or operating Landlink hardware.',
    ],
  },
  {
    title: "Hardware ownership",
    paragraphs: [
      "When you purchase Landlink Module I or Landlink I, the hardware is yours. You decide when it runs, where it runs, and when it stops. We cannot remotely disable, brick, or meter your device.",
      "Specifications, release dates, and pricing for Landlink I are provisional and may change before launch.",
    ],
  },
  {
    title: "Use of the service",
    paragraphs: [
      "You agree to use the site and hardware in compliance with all applicable laws, including local regulations for radio transmission, spectrum use, and aviation. You are responsible for obtaining any licenses or permits required in your jurisdiction.",
      "Do not use Landlink to harm, surveil without consent, or interfere with other communication systems. Do not attempt to disrupt the site itself.",
    ],
  },
  {
    title: "Firmware and updates",
    paragraphs: [
      "Firmware images are provided as-is. We sign firmware so you can verify its origin before flashing. You are responsible for deciding when to apply an update.",
      "Because there is no account, we cannot force an update on your device. A firmware choice is yours to make.",
    ],
  },
  {
    title: "Intellectual property",
    paragraphs: [
      "The Landlink name, logo, site content, and software are owned by cartesiancs or its licensors. You may not copy, redistribute, or create derivative works from the site without our written permission, except where such use is permitted by law.",
      "Open-source components included in our firmware or web app remain governed by their respective licenses.",
    ],
  },
  {
    title: "No warranty",
    paragraphs: [
      "The site and web app are provided 'as is' and 'as available', without warranties of any kind, express or implied, including merchantability, fitness for a particular purpose, and non-infringement.",
      "Landlink is designed for general connectivity in environments where satellite and cellular coverage is limited. It is not certified for life-safety, emergency dispatch, medical telemetry, or any mission-critical use. Do not rely on it as your sole communication channel in such scenarios.",
    ],
  },
  {
    title: "Limitation of liability",
    paragraphs: [
      "To the maximum extent permitted by law, cartesiancs is not liable for any indirect, incidental, special, consequential, or punitive damages arising from your use of the site, web app, or hardware. Our total liability for any claim is limited to the amount you paid us for the hardware in question during the twelve months preceding the claim.",
    ],
  },
  {
    title: "Termination",
    paragraphs: [
      "You can stop using the site at any time. Closing the tab ends any active pairing session. Because there is no account, there is nothing further to cancel.",
      "We may suspend or discontinue parts of the site (for example, the firmware CDN) if required for security, legal, or operational reasons. Your existing hardware will continue to work with the firmware already installed.",
    ],
  },
  {
    title: "Changes to these terms",
    paragraphs: [
      "We may update these terms from time to time. Material changes will be reflected here with an updated date. Continued use of the site after changes constitutes acceptance.",
    ],
  },
  {
    title: "Governing law",
    paragraphs: [
      "These terms are governed by the laws of the Republic of Korea, without regard to conflict-of-law rules. Any dispute will be brought in the courts located in Seoul, Republic of Korea, unless a mandatory law in your jurisdiction provides otherwise.",
    ],
  },
  {
    title: "Contact",
    paragraphs: [
      "Questions about these terms? Reach us at jun@cartesiancs.com.",
    ],
  },
];

export function TermsPage() {
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
        <h1 className="text-base font-medium">Terms</h1>
      </header>

      <section className="px-4 pt-2 pb-4">
        <h2 className="font-display text-3xl leading-tight tracking-tight">
          Terms of
          <br />
          service
        </h2>
        <p className="mt-3 text-xs text-muted-foreground">
          Last updated: {LAST_UPDATED}
        </p>
      </section>

      <section className="flex flex-col gap-6 px-4 pb-10">
        {SECTIONS.map((section) => (
          <article key={section.title} className="flex flex-col gap-2">
            <h3 className="text-sm font-medium">{section.title}</h3>
            {section.paragraphs.map((paragraph, index) => (
              <p
                key={index}
                className="text-sm leading-relaxed text-muted-foreground"
              >
                {paragraph}
              </p>
            ))}
          </article>
        ))}
      </section>

      <footer className="mt-auto border-t border-border px-4 pt-6 pb-[max(env(safe-area-inset-bottom),1.5rem)] text-xs text-muted-foreground">
        See also{" "}
        <Link
          to={ROUTES.privacy}
          className="underline underline-offset-2 hover:text-foreground"
        >
          Privacy Policy
        </Link>
        .
      </footer>
    </main>
  );
}

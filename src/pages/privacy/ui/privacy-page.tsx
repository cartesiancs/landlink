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
    title: "The short version",
    paragraphs: [
      "Landlink is built so that we cannot see your data even if we wanted to. There are no user accounts, no cloud profiles, and no telemetry from your hardware back to us. Configuration, keys, and logs live on your device and on the hardware you own.",
      "If you walk away from the web app, there is nothing for us to hold onto. That is the design, not an after-thought.",
    ],
  },
  {
    title: "What we do not collect",
    paragraphs: [
      "We do not collect your name, email, phone number, or any login credentials, because there is no login.",
      "We do not collect your network keys, device identifiers, mesh topology, firmware logs, or location data from your Landlink hardware. These never leave the device.",
    ],
  },
  {
    title: "What your browser sends",
    paragraphs: [
      "When you open landlink.sh, our CDN receives standard web request metadata — IP address, user agent, requested path, and timestamp — the same information any website receives. This is used for delivering the page and for aggregate, short-retention traffic debugging. It is not tied to a profile of you.",
      "Firmware images are fetched from the same CDN like any other static asset. The request does not carry identifiers about which specific module you are flashing.",
    ],
  },
  {
    title: "Local storage on your device",
    paragraphs: [
      "The web app may store small preferences (for example, the last radio band you selected) in your browser's localStorage so the next visit is less repetitive. This data never leaves your browser. Clearing site data removes it.",
      "Pairing with Landlink hardware uses the Web Bluetooth API. The permission is scoped to the tab and ends when the tab closes.",
    ],
  },
  {
    title: "Your hardware, your data",
    paragraphs: [
      "Network keys are generated on-device. You can rotate or wipe them from the web app at any time. We cannot recover them for you — a factory reset is the recovery path, and that is by design.",
      "Logs written to the module are accessible to you over the local pairing session. We do not have a backdoor to read them.",
    ],
  },
  {
    title: "Third parties",
    paragraphs: [
      "We do not share data with advertisers or data brokers. The only third party in the request path is our CDN provider, which processes web request metadata on our behalf to serve the site.",
      "External links (for example, to our GitHub or company site) take you to services that have their own privacy policies. Review those separately.",
    ],
  },
  {
    title: "Children",
    paragraphs: [
      "Landlink is not directed to children under 13. We do not knowingly collect information about anyone, but this category of user is explicitly out of scope.",
    ],
  },
  {
    title: "Changes to this policy",
    paragraphs: [
      "If we make a material change to how the site or hardware handles data, we will update this page and change the 'last updated' date. Because there is no account, we cannot email you — so if privacy details matter to you, check back here before upgrades.",
    ],
  },
  {
    title: "Contact",
    paragraphs: [
      "Questions about privacy? Reach us at jun@cartesiancs.com. We read and respond personally.",
    ],
  },
];

export function PrivacyPage() {
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
        <h1 className="text-base font-medium">Privacy</h1>
      </header>

      <section className="px-4 pt-2 pb-4">
        <h2 className="font-display text-3xl leading-tight tracking-tight">
          Privacy
          <br />
          policy
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
          to={ROUTES.terms}
          className="underline underline-offset-2 hover:text-foreground"
        >
          Terms of Service
        </Link>
        .
      </footer>
    </main>
  );
}

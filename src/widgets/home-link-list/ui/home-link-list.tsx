import { ChevronRight } from "lucide-react";
import { Link } from "react-router-dom";

import { ROUTES } from "@/shared/config";

type LinkEntry = {
  id: string;
  label: string;
  href: string;
  kind: "external" | "internal";
};

const LINKS: readonly LinkEntry[] = [
  {
    id: "github",
    label: "GitHub",
    href: "https://github.com/cartesiancs",
    kind: "external",
  },
  {
    id: "hardware-setup",
    label: "Hardware Setup",
    href: ROUTES.hardwareSetup,
    kind: "internal",
  },
  {
    id: "company",
    label: "Company",
    href: "https://cartesiancs.com",
    kind: "external",
  },
];

export function HomeLinkList() {
  return (
    <section
      aria-label="Resources"
      className="overflow-hidden rounded-2xl border border-border bg-card"
    >
      <ul className="divide-y divide-border">
        {LINKS.map((link) => {
          const rowClassName =
            "flex items-center justify-between px-4 py-4 text-sm font-medium transition-colors hover:bg-muted";
          return (
            <li key={link.id}>
              {link.kind === "internal" ? (
                <Link to={link.href} className={rowClassName}>
                  <span>{link.label}</span>
                  <ChevronRight className="size-4 text-muted-foreground" />
                </Link>
              ) : (
                <a
                  href={link.href}
                  target="_blank"
                  rel="noreferrer noopener"
                  className={rowClassName}
                >
                  <span>{link.label}</span>
                  <ChevronRight className="size-4 text-muted-foreground" />
                </a>
              )}
            </li>
          );
        })}
      </ul>
    </section>
  );
}

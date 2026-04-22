import { ChevronRight } from "lucide-react";

type LinkEntry = {
  id: string;
  label: string;
  href: string;
  external?: boolean;
};

const LINKS: readonly LinkEntry[] = [
  {
    id: "github",
    label: "GitHub",
    href: "https://github.com/cartesiancs",
    external: true,
  },
  { id: "hardware-setup", label: "Hardware Setup", href: "#hardware-setup" },
  { id: "company", label: "Company", href: "https://cartesiancs.com" },
];

export function HomeLinkList() {
  return (
    <section
      aria-label="Resources"
      className="overflow-hidden rounded-2xl border border-border bg-card"
    >
      <ul className="divide-y divide-border">
        {LINKS.map((link) => (
          <li key={link.id}>
            <a
              href={link.href}
              {...(link.external === true
                ? { target: "_blank", rel: "noreferrer noopener" }
                : {})}
              className="flex items-center justify-between px-4 py-4 text-sm font-medium transition-colors hover:bg-muted"
            >
              <span>{link.label}</span>
              <ChevronRight className="size-4 text-muted-foreground" />
            </a>
          </li>
        ))}
      </ul>
    </section>
  );
}

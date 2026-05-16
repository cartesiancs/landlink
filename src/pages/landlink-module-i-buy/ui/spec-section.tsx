import type { SpecGroup } from "../model/specs";
import { SpecOptionCard } from "./spec-option-card";

type SpecSectionProps = {
  group: SpecGroup;
  selectedId: string | undefined;
  onSelect: (optionId: string) => void;
};

export function SpecSection({ group, selectedId, onSelect }: SpecSectionProps) {
  return (
    <section
      aria-labelledby={`spec-${group.id}-title`}
      className="border-t border-border pt-8 first:border-t-0 first:pt-0"
    >
      <h2
        id={`spec-${group.id}-title`}
        className="font-display text-2xl leading-tight tracking-tight"
      >
        {group.title}
      </h2>
      {group.caption ? (
        <p className="mt-2 text-sm text-muted-foreground">{group.caption}</p>
      ) : null}
      <div
        role="radiogroup"
        aria-labelledby={`spec-${group.id}-title`}
        className="mt-5 space-y-3"
      >
        {group.options.map((option) => (
          <SpecOptionCard
            key={option.id}
            option={option}
            selected={selectedId === option.id}
            onSelect={() => {
              onSelect(option.id);
            }}
          />
        ))}
      </div>
    </section>
  );
}

import type { ReactNode } from "react";

import { Button, Reveal } from "@/shared/ui";
import { hapticTick } from "@/shared/lib";

export type HomeStepAction = {
  label: string;
  onClick?: () => void;
};

export type HomeStepProps = {
  step: number;
  title: string;
  description: string;
  media: ReactNode;
  action?: HomeStepAction;
};

export function HomeStep({
  step,
  title,
  description,
  media,
  action,
}: HomeStepProps) {
  return (
    <div className="flex h-full flex-col px-4 pt-8 pb-4">
      <Reveal>
        <h2 className="text-3xl font-normal leading-tight tracking-tight">
          {step}/ {title}
        </h2>
      </Reveal>

      <Reveal className="mt-3" delay={100}>
        <p className="text-sm leading-relaxed text-muted-foreground">
          {description}
        </p>
      </Reveal>

      <Reveal className="mt-6 min-h-0 flex-1" delay={200}>
        <div className="relative flex h-full items-center justify-center overflow-hidden rounded-2xl bg-background">
          {media}
          {action ? (
            <Button
              size="default"
              variant="outline"
              className="absolute inset-x-4 bottom-4 h-12 text-base"
              onClick={() => {
                hapticTick();
                action.onClick?.();
              }}
            >
              {action.label}
            </Button>
          ) : null}
        </div>
      </Reveal>
    </div>
  );
}

import { createContext, useContext, useEffect } from "react";

export type StepAction = {
  label?: string;
  pending?: boolean;
  disabled?: boolean;
  onAction?: () => void | Promise<void>;
};

export const EMPTY_ACTION: StepAction = {};

export type StepActionWriter = {
  setAction: (action: StepAction) => void;
  clearAction: () => void;
};

export const StepActionReadContext = createContext<StepAction>(EMPTY_ACTION);
export const StepActionWriteContext = createContext<StepActionWriter | null>(
  null,
);

export function useStepAction(): StepAction {
  return useContext(StepActionReadContext);
}

export function useSetStepAction(action: StepAction): void {
  const writer = useContext(StepActionWriteContext);
  const label = action.label;
  const pending = action.pending;
  const disabled = action.disabled;
  const onAction = action.onAction;

  useEffect(() => {
    if (!writer) return;
    const next: StepAction = {};
    if (label !== undefined) next.label = label;
    if (pending !== undefined) next.pending = pending;
    if (disabled !== undefined) next.disabled = disabled;
    if (onAction !== undefined) next.onAction = onAction;
    writer.setAction(next);
    return () => {
      writer.clearAction();
    };
  }, [writer, label, pending, disabled, onAction]);
}

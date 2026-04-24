import { useMemo, useState, type ReactNode } from "react";

import {
  EMPTY_ACTION,
  StepActionReadContext,
  StepActionWriteContext,
  type StepAction,
  type StepActionWriter,
} from "./step-action";

type StepActionProviderProps = {
  children: ReactNode;
};

export function StepActionProvider({ children }: StepActionProviderProps) {
  const [action, setAction] = useState<StepAction>(EMPTY_ACTION);

  const writer = useMemo<StepActionWriter>(
    () => ({
      setAction: (next) => {
        setAction(next);
      },
      clearAction: () => {
        setAction(EMPTY_ACTION);
      },
    }),
    [],
  );

  return (
    <StepActionWriteContext.Provider value={writer}>
      <StepActionReadContext.Provider value={action}>
        {children}
      </StepActionReadContext.Provider>
    </StepActionWriteContext.Provider>
  );
}

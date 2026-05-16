import { useCallback, useMemo, useState } from "react";

import { BASE_PRICE, SPEC_GROUPS } from "./specs";

export type Selections = Readonly<Record<string, string>>;

type Configurator = {
  selections: Selections;
  total: number;
  monthly: number;
  select: (groupId: string, optionId: string) => void;
};

export function useConfigurator(): Configurator {
  const [selections, setSelections] = useState<Selections>(() =>
    Object.fromEntries(SPEC_GROUPS.map((g) => [g.id, g.defaultOptionId])),
  );

  const total = useMemo(() => {
    let sum = BASE_PRICE;
    for (const group of SPEC_GROUPS) {
      const optionId = selections[group.id];
      if (!optionId) continue;
      const option = group.options.find((o) => o.id === optionId);
      if (option) sum += option.priceDelta;
    }
    return sum;
  }, [selections]);

  const select = useCallback((groupId: string, optionId: string) => {
    setSelections((prev) => ({ ...prev, [groupId]: optionId }));
  }, []);

  return {
    selections,
    total,
    monthly: total / 12,
    select,
  };
}

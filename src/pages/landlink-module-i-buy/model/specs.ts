export type SpecOption = {
  id: string;
  label: string;
  description?: string;
  priceDelta: number;
  badge?: string;
};

export type SpecGroup = {
  id: string;
  title: string;
  caption?: string;
  options: readonly SpecOption[];
  defaultOptionId: string;
};

export const BASE_PRICE = 99;

export const SPEC_GROUPS: readonly SpecGroup[] = [
  {
    id: "frequency",
    title: "Frequency band.",
    caption: "Pick the LoRa band that matches your region.",
    defaultOptionId: "freq-915",
    options: [
      {
        id: "freq-915",
        label: "915 MHz",
        description: "North America, Australia, parts of Asia.",
        priceDelta: 0,
      },
      {
        id: "freq-868",
        label: "868 MHz",
        description: "Europe, Middle East, Africa.",
        priceDelta: 0,
      },
    ],
  },
  {
    id: "bundle",
    title: "Choose your kit.",
    caption: "The more nodes you place, the denser your mesh.",
    defaultOptionId: "bundle-2pack",
    options: [
      {
        id: "bundle-single",
        label: "Single node",
        description: "One Module I. Great for trying the mesh on for size.",
        priceDelta: 0,
      },
      {
        id: "bundle-2pack",
        label: "2-Pack",
        description:
          "Two nodes. The minimum for a personal relay between rooms.",
        priceDelta: 79,
        badge: "Most popular",
      },
      {
        id: "bundle-4pack",
        label: "4-Pack",
        description:
          "Four nodes. Cover a neighborhood block or a small campus.",
        priceDelta: 199,
      },
    ],
  },
];

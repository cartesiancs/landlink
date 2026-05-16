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
    defaultOptionId: "bundle-single",
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
        description: "Two nodes. The minimum for a personal relay between rooms.",
        priceDelta: 79,
      },
      {
        id: "bundle-4pack",
        label: "4-Pack",
        description: "Four nodes. Cover a neighborhood block or a small campus.",
        priceDelta: 199,
        badge: "Most popular",
      },
      {
        id: "bundle-starter",
        label: "Mesh Starter Kit (6)",
        description: "Six nodes plus a soft sleeve for each. Built for groups.",
        priceDelta: 329,
        badge: "Best value",
      },
    ],
  },
  {
    id: "antenna",
    title: "Antenna.",
    caption: "Stock works for most. Upgrade if you need extra reach.",
    defaultOptionId: "ant-stock",
    options: [
      {
        id: "ant-stock",
        label: "Stock 2 dBi",
        description: "Pocket-friendly. Up to 8 km line of sight.",
        priceDelta: 0,
      },
      {
        id: "ant-high",
        label: "High-gain 5 dBi",
        description: "A bit taller. Up to 12 km line of sight.",
        priceDelta: 19,
      },
      {
        id: "ant-long",
        label: "Long-range 8 dBi",
        description: "Best for rural and rooftop setups. Up to 20 km line of sight.",
        priceDelta: 39,
      },
    ],
  },
  {
    id: "case",
    title: "Carry case.",
    caption: "Keep Module I safe between adventures.",
    defaultOptionId: "case-none",
    options: [
      {
        id: "case-none",
        label: "No case",
        description: "Travel light.",
        priceDelta: 0,
      },
      {
        id: "case-soft",
        label: "Soft sleeve",
        description: "Slim woven sleeve. Slides into any pocket.",
        priceDelta: 15,
      },
      {
        id: "case-rugged",
        label: "Rugged shell",
        description: "Drop-tested polycarbonate with a carabiner loop.",
        priceDelta: 29,
      },
    ],
  },
  {
    id: "care",
    title: "Care plan.",
    caption: "Accidents happen. Plans cover drops, water, and electrical faults.",
    defaultOptionId: "care-none",
    options: [
      {
        id: "care-none",
        label: "Standard 1-year warranty",
        description: "Covers manufacturer defects. Included with every Module I.",
        priceDelta: 0,
      },
      {
        id: "care-standard",
        label: "Care Standard. 2 years",
        description: "Adds accidental damage cover, two incidents per year.",
        priceDelta: 19,
      },
      {
        id: "care-pro",
        label: "Care Pro. 3 years",
        description: "Unlimited accidental damage plus next-day replacement.",
        priceDelta: 39,
      },
    ],
  },
];

export const ROUTES = {
  home: "/",
  connectBluetooth: "/connect/bluetooth",
  connectUnsupported: "/connect/unsupported",
  connectWifi: "/connect/wifi",
  connecting: "/connecting",
  about: "/about",
  faq: "/faq",
  landlinkOne: "/landlink-one",
  landlinkModuleI: "/landlink-module-i",
  hardwareSetup: "/hardware-setup",
  privacy: "/privacy",
  terms: "/terms",
  lists: "/lists",
  settings: "/settings",
  notFound: "*",
} as const;

export type RoutePath = (typeof ROUTES)[keyof typeof ROUTES];

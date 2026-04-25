export const ROUTES = {
  home: "/",
  connectBluetooth: "/connect/bluetooth",
  connectUnsupported: "/connect/unsupported",
  connectWifi: "/connect/wifi",
  connecting: "/connecting",
  about: "/about",
  faq: "/faq",
  purchase: "/purchase",
  landlinkModuleI: "/purchase/landlink-module-i",
  hardwareSetup: "/hardware-setup",
  privacy: "/privacy",
  terms: "/terms",
  notFound: "*",
} as const;

export type RoutePath = (typeof ROUTES)[keyof typeof ROUTES];

export const ROUTES = {
  home: "/",
  connectBluetooth: "/connect/bluetooth",
  connectWifi: "/connect/wifi",
  connecting: "/connecting",
  about: "/about",
  faq: "/faq",
  purchase: "/purchase",
  hardwareSetup: "/hardware-setup",
  privacy: "/privacy",
  terms: "/terms",
  notFound: "*",
} as const;

export type RoutePath = (typeof ROUTES)[keyof typeof ROUTES];

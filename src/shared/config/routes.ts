export const ROUTES = {
  home: "/",
  connectBluetooth: "/connect/bluetooth",
  connectWifi: "/connect/wifi",
  connecting: "/connecting",
  about: "/about",
  faq: "/faq",
  purchase: "/purchase",
  notFound: "*",
} as const;

export type RoutePath = (typeof ROUTES)[keyof typeof ROUTES];

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
  deviceDashboard: "/device",
  settings: "/settings",
  settingsTheme: "/settings/theme",
  settingsDebug: "/settings/debug",
  settingsReset: "/settings/reset",
  notFound: "*",
} as const;

export type RoutePath = (typeof ROUTES)[keyof typeof ROUTES];

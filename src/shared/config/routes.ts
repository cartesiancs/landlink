export const ROUTES = {
  home: "/",
  about: "/about",
  notFound: "*",
} as const;

export type RoutePath = (typeof ROUTES)[keyof typeof ROUTES];

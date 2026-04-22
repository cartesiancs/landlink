# Vision — Project Guide for Claude

This project **strictly** follows Feature-Sliced Design (FSD). Every change must
preserve layer boundaries, slice encapsulation, and TypeScript strictness. If
a task seems to require breaking these rules, stop and ask the user first.

---

## 1. Stack snapshot

- React 19 + TypeScript (all strict flags on)
- Vite 8 bundler with `@/*` → `./src/*` alias
- React Router v6 (`createBrowserRouter` + `RouterProvider`)
- Tailwind v4 + shadcn/ui components (configured to emit into `src/shared/ui`)
- ESLint 9 flat config with layer-boundary and slice-encapsulation rules

Entry points:
- HTML: [index.html](index.html)
- Bootstrap: [src/main.tsx](src/main.tsx) → mounts `<App />`
- App composition: [src/app/index.tsx](src/app/index.tsx)

---

## 2. FSD layer hierarchy — memorize this

```
app      ← top: providers, router, global styles. The only layer that imports pages.
pages    ← route-level compositions. One slice per route.
widgets  ← large, self-contained UI blocks (e.g. Sidebar, Header) composed from features/entities.
features ← user interactions that deliver business value (e.g. AuthByEmail, AddToCart).
entities ← business entities with their data model & presentation (e.g. User, Product).
shared   ← bottom: reusable infrastructure with no business logic (ui kit, lib, config, api).
```

### Import rule (enforced by ESLint)

**A layer may import only from layers strictly below it.** Never upward, never sideways across slices at the same non-`shared` layer.

| In layer    | May import from                              |
|-------------|----------------------------------------------|
| `app`       | `pages`, `widgets`, `features`, `entities`, `shared` |
| `pages`     | `widgets`, `features`, `entities`, `shared`  |
| `widgets`   | `features`, `entities`, `shared`             |
| `features`  | `entities`, `shared`                         |
| `entities`  | `shared`                                     |
| `shared`    | `shared` only (and node_modules)             |

Two features **must not** import each other. If they need to share logic, lift it down into `entities` or `shared`. Same rule for pages, widgets, entities.

---

## 3. Slice & segment anatomy

A **slice** is one business concern inside a layer (e.g. `features/auth-by-email`).
Slice names are kebab-case.

Inside a slice, use these **segments** as needed:

| Segment | Purpose                                                 |
|---------|---------------------------------------------------------|
| `ui/`   | React components                                        |
| `model/`| State, stores, selectors, business logic, types         |
| `api/`  | Network calls, request builders, response schemas       |
| `lib/`  | Slice-internal helpers that are not worth extracting    |
| `config/` | Constants, feature flags specific to the slice        |

`shared/` is flatter — it typically has `ui/`, `lib/`, `config/`, `api/` directly under it without a slice layer, because the code there is generic.

### Public API (index.ts) — **non-negotiable**

Every slice exposes a single `index.ts` at its root. Outside code imports **only** from that file:

```ts
// ✅ good — hits the public API
import { HomePage } from "@/pages/home";
import { Button } from "@/shared/ui";

// ❌ bad — reaches into slice internals, ESLint will reject
import { HomePage } from "@/pages/home/ui/home-page";
import { Button } from "@/shared/ui/button";
```

The `index.ts` re-exports only what's meant to be public. Anything not re-exported is private to the slice and must stay that way.

---

## 4. Current layout

```
src/
├── app/
│   ├── providers/app-providers.tsx   # StrictMode + future context providers
│   ├── router/app-router.tsx         # createBrowserRouter + RouterProvider
│   ├── styles/index.css              # Tailwind entry + design tokens
│   └── index.tsx                     # <App /> = providers wrapping router
├── pages/
│   ├── home/{ui/home-page.tsx, index.ts}
│   ├── about/{ui/about-page.tsx, index.ts}
│   └── not-found/{ui/not-found-page.tsx, index.ts}
├── widgets/   # empty — populate as widgets emerge
├── features/  # empty — populate as features emerge
├── entities/  # empty — populate as entities emerge
├── shared/
│   ├── ui/            # shadcn components + public API index.ts
│   ├── lib/           # utils.ts (cn helper) + future helpers, via index.ts
│   ├── config/        # ROUTES + other app-wide constants
│   └── api/           # (empty) HTTP clients, request primitives
└── main.tsx           # Vite entry — renders <App />
```

---

## 5. Routing

Routes are declared centrally in [src/app/router/app-router.tsx](src/app/router/app-router.tsx), using path constants from [src/shared/config/routes.ts](src/shared/config/routes.ts).

**When adding a new page:**

1. Add the path constant to `src/shared/config/routes.ts` (`ROUTES.foo = "/foo"`).
2. Create `src/pages/foo/ui/foo-page.tsx` exporting `FooPage`.
3. Re-export from `src/pages/foo/index.ts`: `export { FooPage } from "./ui/foo-page";`.
4. Register the route in `src/app/router/app-router.tsx`.

Never hardcode route strings inside pages/widgets/features — always reference `ROUTES.*`.

---

## 6. TypeScript strictness

All strict flags are enabled in [tsconfig.app.json](tsconfig.app.json). Notable ones that will bite if you're sloppy:

- `strict: true` (the whole bundle)
- `noUncheckedIndexedAccess` — array/record access yields `T | undefined`
- `exactOptionalPropertyTypes` — `{ x?: string }` ≠ `{ x: string | undefined }`
- `noPropertyAccessFromIndexSignature` — use bracket access for dynamic keys
- `noImplicitReturns`, `noImplicitOverride`, `noFallthroughCasesInSwitch`
- `useUnknownInCatchVariables` — `catch (e)` is `unknown`, narrow before use
- `verbatimModuleSyntax` — type-only imports must use `import type`
- `erasableSyntaxOnly` — no enums, no namespaces, no parameter-property shorthand
- `noUnusedLocals`, `noUnusedParameters`

**Do not** add `any`, `// @ts-ignore`, or `as unknown as X` to paper over these. Fix the root cause. The only acceptable escape is `as const` or a narrow, documented type assertion with a `// WHY:` comment.

---

## 7. ESLint rules worth internalizing

Configured in [eslint.config.js](eslint.config.js):

- **Layer boundaries**: automatic per-layer `no-restricted-imports` patterns reject upward imports.
- **Slice encapsulation**: deep imports like `@/features/foo/model/xyz` are rejected — go through the public API.
- **No parent-relative imports** (`../`, `../../`): use the `@/` alias so the target layer is always visible.
- **typescript-eslint `recommendedTypeChecked` + `stylisticTypeChecked`**: includes floating-promises, misused-promises, switch-exhaustiveness, consistent-type-imports.
- `eqeqeq`, `no-console` (warn except warn/error).

Run `npm run lint` before declaring a task done. Lint failures are not optional.

---

## 8. Adding code — decision flow

1. **Is it a route screen?** → `pages/<slug>/`.
2. **Is it a user interaction delivering a verb** (login, add-to-cart, toggle-theme)? → `features/<slug>/`.
3. **Is it a noun with data** (user, post, product)? → `entities/<slug>/`.
4. **Is it a composed UI block on a page**, not tied to one feature (page header, sidebar)? → `widgets/<slug>/`.
5. **Is it framework-agnostic, reusable, no business logic** (ui kit, cn helper, route constants, http client)? → `shared/<segment>/`.
6. **Is it app-wide wiring** (router, providers, global styles)? → `app/`.

When unsure between two layers, pick the **lower** one — it's easier to promote than demote.

---

## 9. shadcn/ui integration

`components.json` is configured so `npx shadcn add <name>` drops components into `src/shared/ui/`. After generation:

1. The generated component may import `cn` from `@/shared/lib/utils` (shadcn default). That's fine — the public API at `@/shared/lib` also re-exports `cn`. Outside the `shared` layer always import from `@/shared/lib`.
2. Re-export the public symbols from [src/shared/ui/index.ts](src/shared/ui/index.ts).
3. Consumers must import from `@/shared/ui`, never `@/shared/ui/<component>`.

---

## 10. Commands

```
npm run dev      # Vite dev server
npm run build    # tsc -b && vite build — must pass with zero TS errors
npm run lint     # ESLint flat config — must pass
npm run preview  # preview the production build
```

Always run `npm run build` and `npm run lint` before handing work back. A task is not done if either fails.

---

## 11. What NOT to do

- ❌ Don't add files under `src/components/` or `src/lib/` — those paths are gone.
- ❌ Don't import `@/shared/ui/button` directly — use `@/shared/ui`.
- ❌ Don't put business logic in `shared/`. `shared/` is reusable infrastructure only.
- ❌ Don't import one feature from another. Extract shared logic downward.
- ❌ Don't bypass the router — every route-level screen belongs in `pages/`.
- ❌ Don't weaken tsconfig strictness or disable ESLint rules globally to make an error go away. Fix the code.
- ❌ Don't introduce a new top-level layer. The six layers above are the whole hierarchy.

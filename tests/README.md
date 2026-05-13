# Tests

This repository hosts two independent codebases (the React web app and the
ESP32 firmware), each with its own test runner. This document describes where
tests live and how to run them.

## Layout

```
.
├── tests/             # Cross-cutting tests (e2e, integration scenarios)
├── src/               # Web app source with co-located unit tests (*.test.ts(x))
└── firmware/test/     # PlatformIO native unit tests (Unity framework)
```

## `tests/` (this folder)

Top-level home for cross-cutting tests that do not belong to a single
Feature-Sliced Design slice: end-to-end scenarios, multi-slice integration
flows, and shared fixtures.

Tests placed here are picked up automatically by Vitest via the default include
pattern (`**/*.{test,spec}.?(c|m)[jt]s?(x)`) defined in
[`vitest.config.ts`](../vitest.config.ts). The `@/*` alias maps to
[`src/*`](../src), so imports from FSD slices work the same way they do inside
`src/`.

Run from the repository root:

```bash
npm run test               # one-shot run
npm run test:watch         # watch mode
npm run test -- tests/     # restrict the run to this folder
```

The folder is currently empty. Add tests as `.test.ts` or `.test.tsx` files
(optionally grouped by scenario: `tests/integration/`, `tests/e2e/`, etc.).

## `src/` — web app unit tests

Unit tests are co-located with the code they exercise, one `*.test.ts(x)` next
to each unit under test. This keeps tests inside the FSD slice that owns them
so the public-API boundary is enforced for tests just like production code.

The Vitest config wires up `jsdom`, Testing Library, and a global setup file at
[`src/test/setup.ts`](../src/test/setup.ts).

```bash
npm run test                       # run all unit + integration tests once
npm run test:watch                 # watch mode (interactive filter)
npm run test -- src/features/      # restrict to a single layer or slice
npm run test -- -t "renders"       # filter by test name pattern
```

A task is not done until `npm run test`, `npm run lint`, and `npm run build`
all pass.

## `firmware/test/` — ESP32 firmware tests

The firmware uses [PlatformIO](https://platformio.org/) with the
[Unity](https://www.throwtheswitch.org/unity) test framework. Tests run
natively on the host (no board required) under the `native-test` environment
defined in [`firmware/platformio.ini`](../firmware/platformio.ini).

Each test lives in its own `firmware/test/test_<name>/test_main.cpp`, pulls in
the source header it covers, and includes the matching `.cpp` directly so the
native build stays decoupled from the Arduino/ESP-IDF source tree.

Run from the `firmware/` directory:

```bash
cd firmware
pio test -e native-test                                # run every native test
pio test -e native-test -f test_meshtastic_frame       # run a single test folder
pio test -e native-test -v                             # verbose Unity output
```

On-target tests (i.e. running on the actual T-Beam board) are not configured;
the `native-test` environment is for pure-logic modules only (codec,
deduplication, CRC, etc.).

## CI quick reference

| Scope                | Command                          | Working directory |
| -------------------- | -------------------------------- | ----------------- |
| Web app + `tests/`   | `npm run test`                   | repo root         |
| Type check + build   | `npm run build`                  | repo root         |
| Lint                 | `npm run lint`                   | repo root         |
| Firmware native unit | `pio test -e native-test`        | `firmware/`       |

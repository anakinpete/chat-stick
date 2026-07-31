# Roadmap

## Phase 0 — Working technical foundation

Status: substantially complete.

- Build firmware in PlatformIO.
- Upload to M5Stick S3.
- Connect to Wi-Fi.
- Run local backend on port 8787.
- Confirm device-to-backend reachability.

Exit condition: device and backend can communicate using the current development configuration.

## Phase 1 — Persistent configuration and browser provisioning

Next phase.

1. Add a settings abstraction.
2. Store SSID, password, backend address, backend port, and active theme in Preferences.
3. Make startup prefer saved settings.
4. Add setup access point and browser form.
5. Add setup-mode display states.
6. Add a finalized boot-time way to reopen setup.
7. Remove normal dependence on hardcoded credentials.

Exit condition: a fresh or relocated device can be configured entirely from a phone browser and reconnect after restart.

## Phase 2 — Freeze the MVP information model

Status: implementation complete.

1. Define shared header data: time and weather.
2. Define Codex status and usage/progress semantics.
3. Define Meeting/agenda semantics.
4. Resolve the required items in `OPEN_QUESTIONS.md` for these fields.
5. Define loading, empty, offline, warning, success, and error states.

Exit condition: the UI can be driven by stable semantic data without theme-specific assumptions.

## Phase 3 — Plain functional two-screen UI

Status: implemented in firmware; physical display review pending.

1. Implement one plain, highly readable base presentation.
2. Display time and weather in the shared header.
3. Display Codex/status and usage/progress.
4. Display Meeting/agenda information.
5. Confirm normal screen navigation on hardware.
6. Confirm setup and offline states.
7. Freeze the renderer and widget contract for the MVP.

Exit condition: all core information and controls work before themed styling begins.

The plain renderer, isolated demo adapter, and non-blocking meeting marquees
are implemented. The temporary short-Button-A screen toggle is for evaluation
only. Do not freeze the renderer/widget contract or begin Phase 4 until both
screens, marquee timing, setup/menu reachability, and text bounds are reviewed
on hardware.

## Phase 4 — Theme engine and selector

Status: theme contract, manager, persistence, selector, companion input, and
the first NERV visual implementation are complete; remaining visuals pending.

1. Add `ThemeId` and `ThemeManager`.
2. Extract palette, typography, frame, alert, icon, and progress styling.
3. Add persistent active-theme selection.
4. Add runtime hold-both-buttons theme selector.
5. Add safe fallback for invalid saved theme values.
6. Add a basic preview only after selection works reliably.

Exit condition: the plain renderer can switch visual themes without changing data or behavior.

`ThemeId`, `ThemeStyle`, `BaseTheme`, semantic presentation helpers, and
`ThemeManager` are implemented. Plain and NERV are selectable; the other four
product identities appear as `[soon]`. The menu and hold-A+B paths use the same
selector. Runtime factory reset uses the confirmed Device-menu route rather
than the selector chord.

## Phase 5 — Five polished product themes

Implement and finish one at a time:

1. NERV-style
2. Ghost-style HUD
3. Pip-Boy-style
4. Alien / Weyland-Yutani-style
5. Gundam cockpit-style

For each theme:

- all two-screen content is readable;
- all shared states are styled;
- progress and alerts are complete;
- no theme-specific business logic exists;
- build and device test pass before starting the next theme.

NERV now has its first complete portrait-first implementation and is pending
physical visual review. Font scale, spacing, panel height, emblem size,
progress segmentation, marquee bounds, and palette values remain deliberately
centralized for that tuning pass. Ghost HUD, Pip-Boy, Alien / Weyland-Yutani,
and Gundam Cockpit remain unimplemented.

## Phase 6 — Live data integration and resilience

Integrate the agreed core data sources:

- time synchronization;
- weather source;
- Codex status/usage source;
- meeting/calendar source.

Then improve:

- reconnect behavior;
- offline caching where useful;
- backend-unavailable state;
- update intervals;
- power and battery behavior;
- secret-safe logging;
- settings reset and recovery.

## Phase 7 — Optional enhancements

Only after the MVP is stable:

- random theme on boot;
- day/night mode;
- animated transitions;
- optional AI meeting summaries;
- optional assistant/chat capability;
- Omnitool theme;
- Halo theme;
- Blade Runner theme.

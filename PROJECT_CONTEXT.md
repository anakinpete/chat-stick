# M5 Project Context

## Project identity

Build a polished, glanceable sci-fi companion on the M5Stick S3.

The existing Chat Stick firmware and backend are only a technical starting point. The final product is **not primarily an AI chat device** and should not be designed around the repository's original chat flow.

## Agreed product goal

Use:

- one firmware;
- one shared application and data layer;
- one reusable screen renderer;
- multiple selectable visual themes;
- persistent settings stored in ESP32 flash;
- browser-based setup for Wi-Fi and backend details.

Themes change presentation, not the device's core information, navigation model, or business logic.

## Agreed MVP information

The MVP has two main information screens with a shared header:

### Shared header

- current time;
- current weather information.

### Screen 1 — Codex / status

- Codex activity or status information;
- Codex usage or progress information;
- theme-styled progress and warning states where the data requires them.

The exact Codex metric names and source are not yet frozen. Do not invent a backend contract without resolving the items in `docs/OPEN_QUESTIONS.md`.

### Screen 2 — Meeting / agenda

- current or next meeting information;
- meeting or agenda content.

The exact meeting fields and calendar source are not yet frozen.

## Agreed UI direction

The same two-screen structure and data model are retained across themes. Each theme may define:

- colours;
- fonts;
- borders and panel shapes;
- icons;
- lightweight animations;
- boot screen;
- alert style;
- progress-bar style.

The finished product theme set is:

1. **NERV-inspired** — black with orange/red/yellow signals and angular warning panels.
2. **Ghost HUD-inspired** — cyan/blue, thin lines, clean tactical telemetry.
3. **Pip-Boy-inspired** — green monochrome, chunky retro presentation, optional subtle scanlines.
4. **Alien / Weyland-Yutani-inspired** — muted industrial terminal colours, grids, warning stripes, and compact labels.
5. **Gundam cockpit-inspired** — blue/white instrumentation with red/yellow accents and mechanical framing.

Plain is an internal development and safe-fallback theme, not a sixth product
theme. LCARS is not part of the active plan.

## Agreed theme control

During normal operation, hold both buttons for about two seconds to open the theme selector.

```text
SELECT THEME

> Cancel
  Plain *
  NERV
  Ghost HUD [soon]
```

Inside the selector:

- Button B moves to the next item;
- Button A selects;
- the list wraps and Cancel returns without changing the active theme;
- the selected theme is stored in ESP32 Preferences and survives restart.

NERV is the first selectable product theme. Ghost HUD, Pip-Boy, Alien Terminal,
and Gundam Cockpit remain unavailable `[soon]` entries and will be enabled one
at a time as their visual implementations are reviewed.

## Agreed configuration direction

Routine Wi-Fi or server changes must not require editing `credentials.h` and rebuilding firmware.

Preferred flow:

1. If no valid settings exist, the device starts a setup access point.
2. The user connects from a phone or computer.
3. The browser opens the setup page at `http://192.168.4.1`.
4. The user enters Wi-Fi SSID, Wi-Fi password, backend address, and backend port.
5. The device stores the values in ESP32 Preferences.
6. The device restarts and connects normally.

A boot-time gesture may reopen setup mode later; its exact mapping is still to be finalized.

## Development policy

Use Codex for as much substantive implementation as practical, but conserve usage by assigning it **small, isolated, testable jobs**.

Do manually only work that is clearly faster and lower risk, such as:

- one-line corrections;
- obvious naming or syntax fixes;
- running build, upload, and diagnostic commands;
- reviewing diffs;
- updating a value already exposed through configuration.

Do not give Codex a broad request to build the entire product or refactor the whole repository in one pass.

## Current technical state

At the time of this document revision:

- PlatformIO builds the firmware;
- firmware upload to the M5Stick S3 works;
- Wi-Fi connectivity works using development credentials;
- the local backend runs on port `8787`;
- the device has reached the local backend;
- persistent settings and the browser setup portal are implemented;
- the shared UI model, plain renderer, theme contract, manager, and runtime
  selector are implemented;
- the portrait-first NERV visual implementation is available for hardware
  review; the other four product-theme visuals remain unimplemented.

## Immediate next milestone

Review the plain renderer and selector on hardware, then implement and review
the five product themes one at a time without changing the shared data model,
screen flow, or input behavior.

## Deferred until after the MVP

- random theme on boot;
- day/night mode;
- animated screen transitions;
- additional themes beyond the agreed five;
- optional AI summaries or chat features;
- broad smartwatch-style functionality.

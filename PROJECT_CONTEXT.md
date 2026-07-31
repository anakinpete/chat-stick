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

The first release should contain four polished themes:

1. **NERV-style** — orange and black, dense technical warning panels.
2. **Ghost-style HUD** — cyan, thin lines, restrained telemetry.
3. **Pip-Boy-style** — green monochrome, pixel character, optional scanline treatment.
4. **LCARS-style** — rounded coloured panels, strong hierarchy, high readability.

Future candidates include Alien, Omnitool, Gundam, Halo, and Blade Runner.

## Agreed theme control

During normal operation, hold both buttons for about two seconds to open the theme selector.

```text
SELECT THEME

> NERV
  GHOST
  PIP-BOY
  LCARS
```

Inside the selector:

- Button B moves to the next item;
- Button A selects;
- the selected theme is stored in ESP32 Preferences and survives restart.

A theme preview is planned after basic selection works.

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
- the browser setup portal and custom theme engine have not yet been implemented.

## Immediate next milestone

Implement persistent device settings and the browser-based Wi-Fi/backend setup portal while preserving the working firmware and network path.

After that:

1. define and freeze the shared UI data contract;
2. build one plain functional renderer;
3. extract the theme interface and manager;
4. add the four themes one at a time.

## Deferred until after the MVP

- random theme on boot;
- day/night mode;
- animated screen transitions;
- additional themes beyond the initial four;
- optional AI summaries or chat features;
- broad smartwatch-style functionality.

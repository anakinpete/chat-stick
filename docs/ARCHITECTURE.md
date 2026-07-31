# Architecture

## Architectural objective

Separate device behavior from visual presentation so one stable application can support multiple themes without duplicate logic.

## High-level data flow

```text
Buttons / Timers / Network Events
               |
               v
       Controllers and Services
               |
               v
       Shared Application State
               |
               v
      Screen Flow and Renderer
               |
               v
     Active Theme and UI Widgets
               |
               v
             Display
```

## Layer responsibilities

### Input and controllers

Responsible for:

- button events and hold detection;
- current-screen selection;
- opening and controlling the theme selector;
- setup-mode entry;
- translating raw events into application actions.

Not responsible for drawing theme-specific UI.

### Services

Responsible for:

- Wi-Fi connection management;
- browser setup portal;
- backend communication;
- time and weather updates;
- Codex data updates;
- meeting data updates;
- persistence through a settings store.

Services update shared state; they do not draw screens.

### Local Codex allowance boundary

The Cloudflare Worker runtime cannot launch a Windows process. For local
development, a loopback-only Node sidecar invokes the installed official Codex
CLI as `codex app-server --stdio`, completes the JSONL initialization handshake,
and reads `account/read` plus `account/rateLimits/read`. It normalizes only safe
rate-limit fields and never returns the raw account object or authentication
material.

The sidecar caches successful reads for 60 seconds, shares an in-flight refresh,
and returns the last successful result as stale if a later refresh fails. It
uses a short-lived app-server process with a timeout and forced cleanup. The
Worker exposes `/api/codex/allowance`, applies the existing optional device
authorization, and proxies the normalized sidecar response. This is a local
host architecture; a deployed Cloudflare Worker cannot reach the user's
loopback sidecar.

### Shared application state

Contains semantic data needed by the renderer, such as:

- header time and weather;
- Codex status and usage/progress;
- meeting/agenda data;
- connection and error states;
- active screen;
- active theme identifier.

The initial implementation may adapt existing structures rather than replacing them all at once.

### Screen flow and renderer

Responsible for:

- choosing which screen to render;
- preserving the shared information hierarchy;
- mapping application state to reusable widgets;
- providing theme-independent screen behavior;
- invoking visual primitives from the active theme.

The renderer must not perform network requests or store credentials.

The current plain implementation attaches beside the legacy `TextDisplay`
path. `AppController` selects `CompanionRenderer` only for the normal ready
view; boot, setup, menu, alarm, connection, recording, and error states keep
using the established renderer. Both paths share the existing display canvas
and dirty-frame flush.

`CompanionDemoData` is a temporary adapter. It combines real clock, battery,
Wi-Fi, and backend signals with clearly isolated weather, Codex, and meeting
demo content. It can be replaced by provider-backed adapters without changing
the renderer.

`TextMarquee` owns only elapsed-time scroll state. It performs no drawing,
delay, network access, or input handling. `CompanionRenderer` clips marquee
drawing to the meeting title and agenda regions.

### Theme layer

Responsible for visual tokens and visual primitives:

- colours;
- fonts;
- spacing and border treatments;
- panel shapes;
- icons;
- alerts;
- progress indicators;
- boot styling;
- lightweight animation state where necessary.

A theme must not own business data, network clients, or navigation rules.

The implemented contract is a lightweight static `ThemeStyle`, not a renderer
subclass. It contains palette, typography, spacing, border, progress, and
semantic-symbol tokens. `BaseTheme` supplies the safe Plain implementation.
`SemanticPresentation` centrally maps semantic states to clear labels and
theme-selected symbols.

## Layout ownership clarification

The shared renderer owns **semantic layout and information order**. A theme may vary the geometry of a panel or widget inside an allocated region, but it must not rearrange the product into a different application.

This prevents two common failures:

1. a theme becoming a duplicate screen implementation;
2. the renderer becoming full of `if (theme == ...)` branches.

## Recommended folder direction

Adapt incrementally to the existing repository rather than moving the whole codebase at once.

```text
src/
  core/
    AppState.*
    Settings.*
    ThemeManager.*
    ScreenController.*
  input/
    ButtonController.*
  screens/
    CodexScreen.*
    MeetingScreen.*
    ThemeSelectorScreen.*
    SetupStatusScreen.*
  ui/
    Renderer.*
    Widgets.*
  themes/
    Theme.h
    BaseTheme.*
    NervTheme.*
    GhostTheme.*
    PipBoyTheme.*
    AlienTheme.*
    GundamTheme.*
  networking/
    WifiManager.*
    SetupPortal.*
    BackendClient.*
```

File names may be adjusted to fit the existing code. The separation of responsibilities matters more than these exact names.

## Theme interface

`ThemeId` contains Plain, NERV, Ghost HUD, Pip-Boy, Alien Terminal, and Gundam
Cockpit. Stable persistence identifiers are `plain`, `nerv`, `ghost`,
`pip-boy`, `alien`, and `gundam`. Plain is internal and NERV is the first
available product implementation. The other four product identities enumerate
as unavailable until their visual styles are completed. The style contract uses
static token data rather than virtual renderer classes, which fits the current
memory constraints and keeps one Codex/Meeting implementation.

The renderer receives the active `ThemeStyle` for each frame. It owns field
order, layout, state handling, and marquee behavior; the theme owns visual
tokens only.

## Theme manager

The theme manager should:

- expose the current `ThemeId`;
- provide the active theme implementation;
- enumerate available themes for the selector;
- validate saved theme values;
- save a confirmed selection through the settings store;
- fall back to a known base theme when a value is invalid.

These responsibilities are now implemented. Identifier parsing, labels,
enumeration, and availability validation have one descriptor mapping in
`ThemeManager`; `SettingsStore` remains the only Preferences owner. Invalid or
recognized-but-unimplemented persisted identifiers fall back to Plain. The
selector lists all identities, marks unavailable entries `[soon]`, and persists
only a confirmed available selection.

NERV extends the static contract with reusable orientation, composition, theme
mark, corner-cut, side-rail, primary-text-scale, and progress-segment tokens.
`NervTheme` owns only those presentation values. The shared
`CompanionRenderer` owns the reusable stacked composition and primitive drawing
for angular panels, segmented progress, battery treatment, and the compact
local leaf/wordmark approximation. It continues to own all semantic field order
and marquee state.

`TextDisplay` reuses the same 32,400-pixel framebuffer allocation and recreates
its canvas only when crossing between the 240 x 135 landscape system UI and the
135 x 240 portrait NERV companion composition. Existing menu, setup, reset,
connection, and error screens remain landscape and consume the active palette;
they do not gain duplicate NERV state logic.

Normal companion input consumes the A+B hold before individual button actions.
It suppresses events until both buttons are released, preventing screen toggle,
menu opening, or stale selector input. Short A changes the primary screen, hold
B opens the menu, and hold A alone is deliberately inactive. Runtime factory
reset is routed through the Device menu confirmation instead of a destructive
button chord.

## Persistent settings

Use ESP32 Preferences for user settings, including:

### MVP

- Wi-Fi SSID;
- Wi-Fi password;
- backend address;
- backend port;
- active theme.

### Later

- device name;
- brightness;
- timezone;
- 12/24-hour mode;
- metric/imperial units;
- day/night mode;
- random-theme-on-boot.

Use a settings abstraction rather than scattering direct Preferences calls throughout the code.

## Reliability constraints

- Device startup must not depend on the backend being available.
- A temporary network outage must not erase settings.
- Invalid or missing configuration must lead to a recoverable setup state.
- Render code must remain responsive and non-blocking.
- Frequent drawing paths should avoid unnecessary heap allocation.
- Credentials must never appear in normal logs.
- Refactoring must preserve the known working build after each milestone.

## Data contract freeze point

Before implementing the five product theme styles:

1. define the semantic fields for header, Codex, and Meeting data;
2. implement them in a plain base UI;
3. verify both screens on hardware;
4. freeze the renderer/widget contract for the MVP;
5. then add themes one at a time.

This prevents repeated theme rewrites while the data model is still changing.

Steps 1 and 2 are implemented. Hardware verification of both screens and their
text bounds is still required before steps 3 and 4 can freeze the renderer and
widget contract.

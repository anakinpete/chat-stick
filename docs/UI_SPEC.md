# UI Specification

## UI objective

Create one consistent two-screen interface whose visual language can be replaced by themes without changing the information hierarchy or controls.

## Shared screen structure

Every primary screen uses the same semantic layers:

```text
+--------------------------------+
| Time  Weather  Temp  Batt Conn |  Shared header
+--------------------------------+
|                                |
|     Screen-specific content    |  Codex or Meeting
|                                |
+--------------------------------+
| Optional state / progress area |
+--------------------------------+
```

The exact pixel layout may be tuned for the M5Stick S3, but the semantic order remains stable.

## Screen 1 — Codex / status

Required semantic data:

- activity: Idle, Working, Waiting, Complete, Error, or Unavailable;
- current task title;
- optional task progress percentage;
- separate optional Codex allowance remaining percentage;
- optional allowance reset label or time;
- optional last-update time;
- availability and severity states covering loading, stale, warning, success,
  error, and unavailable.

The renderer should accept semantic data rather than preformatted theme-specific strings.
It must never substitute task progress for unknown allowance data.

Conceptual call flow:

```cpp
drawHeader(data.header, activeTheme);
drawCodexScreen(data.codex, activeTheme);
```

The semantic fields are frozen. Backend payload names and live sources are not.

## Screen 2 — Meeting / agenda

Required semantic data:

- current meeting when one is in progress, otherwise the next meeting;
- state: None, Upcoming, InProgress, Finished, or Unavailable;
- full meeting title;
- optional start time;
- optional seconds until start and seconds remaining;
- location or call type;
- one full agenda or summary line;
- availability states covering loading, stale, offline context, unavailable,
  and error.

Conceptual call flow:

```cpp
drawHeader(data.header, activeTheme);
drawMeetingScreen(data.meeting, activeTheme);
```

The model preserves full title and agenda strings. A later renderer may present
long strings with a non-blocking horizontal marquee; it must not truncate them
in the data layer.

## Shared semantic model

The firmware model is theme-independent and contains:

- shared header, Codex/status, and meeting/agenda data;
- the active primary screen;
- typed activity, meeting, connection, availability, severity, weather, and
  location states;
- explicit known/unknown wrappers for optional numeric values and timestamps;
- validated percentage values constrained to `0..100`.

It contains no colours, fonts, geometry, theme identifiers, display calls,
network clients, or backend JSON field names.

## Required system states

These are not extra applications; they are temporary display states within the same UI system.

- booting;
- connecting to Wi-Fi;
- connecting to backend;
- setup mode;
- loading data;
- offline/backend unavailable;
- validation or configuration error;
- successful configuration and restart.

Every theme may style these states, but state meaning and recovery behavior remain shared.

## Controls

### Agreed theme selector control

During normal operation:

- hold Button A and Button B together for approximately two seconds to open the theme selector;
- Button B moves down to the next theme;
- Button A confirms the selected theme;
- selection is written to Preferences.

### Primary-screen navigation

The exact normal-operation mapping for switching between Codex and Meeting screens has not yet been explicitly frozen. Do not silently assign a final mapping in a large implementation task. A temporary mapping may be used only when called out in the task and documented afterward.

### Setup-mode entry

Automatic entry when settings are missing is agreed. A manual boot gesture is planned but not yet frozen. It may reuse the two-button chord at boot because runtime and boot contexts are separate, but this must be decided explicitly before implementation.

## Theme selector

Minimum first version:

```text
SELECT THEME

> NERV
  GHOST
  PIP-BOY
  LCARS
```

Later enhancement:

- preview the highlighted theme before confirmation.

## Theme visual contract

Themes may define:

- colour palette;
- heading and body fonts;
- border style;
- panel shape;
- icon treatment;
- progress bar appearance;
- alert appearance;
- boot presentation;
- lightweight, non-blocking animation.

Themes must not:

- fetch data;
- alter the meaning of data;
- change button behavior;
- add or remove core information;
- own Wi-Fi or backend state;
- create a separate screen flow.

## Initial theme direction

### NERV-style

- black base;
- orange as the dominant signal colour;
- technical labels and warning-panel character;
- strong caution and critical-state treatment;
- dense appearance without sacrificing legibility.

### Ghost-style HUD

- cyan-dominant telemetry;
- thin lines and restrained frames;
- clean spacing;
- precise, quiet, high-tech character.

### Pip-Boy-style

- green monochrome presentation;
- pixel-oriented typography or treatment;
- optional subtle scanlines;
- chunky, readable gauges and progress indicators.

### LCARS-style

- rounded coloured panels;
- clear hierarchy and large readable labels;
- distinctive segmented controls and framing;
- colour used to organize, not decorate randomly.

## Cross-theme consistency rules

- Keep time, weather, battery, and connection state in the shared header.
- Keep Codex and Meeting as the two main screens.
- Keep task progress and Codex allowance as visibly distinct concepts.
- Use the same data states and alert severity meanings.
- Maintain comparable text size and readability.
- Long title and agenda scrolling must be non-blocking.
- Do not let decorative animation delay input or networking.
- Avoid theme-specific wording unless it is purely a label style and does not change meaning.

## Deferred visual features

- full animated transitions between screens;
- random theme on boot;
- day/night variants;
- Alien, Omnitool, Gundam, Halo, and Blade Runner themes.

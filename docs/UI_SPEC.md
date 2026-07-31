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

The model preserves full title and agenda strings. The plain renderer presents
long meeting title and agenda strings with a non-blocking horizontal marquee;
it does not truncate them in the data layer.

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

Normal companion operation uses:

- short Button A toggles Codex and Meeting;
- holding Button A alone performs no action and does not start legacy
  push-to-talk;
- holding Button B opens the companion menu;
- holding A and B together for approximately two seconds opens the theme
  selector without leaking either individual-button action.

### Companion menu

The normal companion menu is:

- Back;
- Codex;
- Meeting;
- Theme;
- Device.

Legacy conversation actions are not presented as primary companion actions.
The Device submenu and setup access remain reachable. Theme opens the same
selector as the A+B chord. Factory reset is available only through the Device
menu with confirmation during normal operation; the runtime two-button chord
does not erase settings.

### Setup-mode entry

Automatic entry when settings are missing is agreed. A manual boot gesture is planned but not yet frozen. It may reuse the two-button chord at boot because runtime and boot contexts are separate, but this must be decided explicitly before implementation.

## Theme selector

Minimum first version:

```text
SELECT THEME

> Cancel
  Plain *
  NERV [soon]
  Ghost HUD [soon]
```

Button B moves and wraps through the complete list. Button A confirms an
available entry. Cancel, or holding Button B to return, closes the selector
without changing the active theme. The active theme has a `*` marker. The five
unfinished product themes are visible as `[soon]` and cannot be selected;
Plain is the currently available internal fallback.

Later enhancement after each visual implementation:

- preview the highlighted theme before confirmation.

## Theme visual contract

The implemented theme contract supplies presentation-only tokens for:

- semantic palette colours;
- typography role and scale;
- spacing;
- section dividers and borders;
- progress-bar treatment;
- ASCII-safe battery, weather, connection, and activity symbols.

The single companion renderer consumes these tokens. Semantic labels are shared
across themes, including `WORKING`, `ACTION REQUIRED`, `DONE`, `ERROR`,
`STALE`, `LOADING`, and `UNAVAILABLE`.

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
- orange/red/yellow signal colours;
- angular technical labels and warning-panel character;
- strong caution and critical-state treatment;
- dense appearance without sacrificing legibility.

### Ghost-style HUD

- cyan-dominant telemetry;
- thin lines and restrained frames;
- clean spacing;
- precise, quiet, high-tech character;
- translucent or radar-like visual language that remains distinct from Pip-Boy.

### Pip-Boy-style

- green monochrome presentation;
- pixel-oriented typography or treatment;
- optional subtle scanlines;
- phosphor/CRT character;
- chunky, readable gauges and progress indicators, intentionally heavier than Ghost HUD.

### Alien / Weyland-Yutani-inspired

- muted amber, green, off-white, and dark industrial colours;
- utilitarian terminal layout with grids and compact labels;
- warning stripes and functional corporate spacecraft-computer character.

### Gundam cockpit-inspired

- blue/white base with red/yellow accents;
- angular cockpit instrumentation and strong system-status framing;
- mechanical and targeting icon language;
- energetic presentation that remains readable.

## Cross-theme consistency rules

- Keep time, weather, battery, and connection state in the shared header.
- Keep Codex and Meeting as the two main screens.
- Keep task progress and Codex allowance as visibly distinct concepts.
- Use the same data states and alert severity meanings.
- Maintain comparable text size and readability.
- Long title and agenda scrolling must be non-blocking.
- Do not let decorative animation delay input or networking.
- Avoid theme-specific wording unless it is purely a label style and does not change meaning.
- Show real product data only; decorative motifs must not invent telemetry.
- Represent unknown values honestly in every theme.
- Support the same semantics in vertical and horizontal compositions where practical.

## Plain renderer implementation

The current base renderer uses the existing 240 x 135 landscape canvas and
8 x 16 platform font. It has one shared header, one screen-specific content
area, and a compact navigation footer.

Meeting title and agenda marquees:

- remain static when the text fits;
- pause for 900 ms before moving;
- advance from elapsed time at 28 pixels per second;
- pause for 800 ms at the end;
- restart cleanly without blocking the main loop;
- clip all drawing to their assigned row.

The Codex task title uses the same non-blocking marquee behavior when it does
not fit. The plain header uses clear `BAT 100%`-style battery text, a compact
weather symbol with temperature, and `OFFLINE`, `WIFI`, `SYNC`, or `ONLINE`
connection labels. Unknown allowance is shown as `ALLOWANCE N/A`, and reset
information is hidden when it has no meaning.

These timing and layout choices remain provisional until physical hardware
review. The semantic model is frozen, but the renderer/widget contract is not
yet permanently frozen.

## Deferred visual features

- full animated transitions between screens;
- random theme on boot;
- day/night variants;
- themes beyond the agreed five-theme product set.

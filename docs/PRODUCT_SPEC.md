# Product Specification

## Product statement

The M5 sci-fi companion is a small wearable or pocketable display that gives the user a fast, themed view of their current operational context.

The MVP focuses on two questions:

1. **What is happening with Codex and its usage?**
2. **What meeting or agenda item matters now or next?**

Time and weather remain visible as shared context.

## Primary function

The device should provide useful information at a glance without requiring the user to open a laptop or phone for every check.

It should feel like a coherent fictional operating system while remaining readable and dependable.

## MVP functional requirements

### Required

- boot reliably;
- connect to saved Wi-Fi;
- connect to the configured backend when available;
- remain usable enough to show status when the backend is unavailable;
- display time and weather in a shared header;
- display Codex status/activity and usage/progress;
- display current or next meeting/agenda information;
- move between the two primary screens;
- open a theme selector with a two-button hold during normal operation;
- save and restore the selected theme;
- support browser-based Wi-Fi and backend configuration;
- present clear setup, loading, offline, and error states.

### Quality requirements

- information must be glanceable on the M5Stick S3 display;
- important warnings must be distinguishable from normal telemetry;
- theme effects must not reduce basic readability;
- rendering must not block connection handling;
- normal network changes must not require source-code edits;
- secrets must not be displayed or logged.

## Two-screen information model

### Screen A — Codex / status

Frozen semantic content:

- activity state: Idle, Working, Waiting, Complete, Error, or Unavailable;
- current task title;
- current task progress percentage when known;
- Codex allowance or credit remaining percentage when known;
- allowance reset text or reset time when known;
- last update time;
- explicit loading, stale, unavailable, warning, success, and error states.

Task progress and Codex allowance are separate values. Task progress describes
the current task; allowance describes remaining Codex usage or credit. Unknown
allowance must remain unknown rather than being represented as zero or another
invented percentage.

Still unresolved:

- live sources for activity, task progress, and allowance;
- backend endpoint and refresh interval;
- warning thresholds and allowance-reset semantics supplied by the source.

### Screen B — Meeting / agenda

Frozen semantic content:

- current meeting, otherwise the next meeting;
- meeting state: none, upcoming, in progress, finished, or unavailable;
- full meeting title;
- start time;
- time until start or time remaining when known;
- location or call type;
- one full agenda or summary line;
- explicit loading, stale, offline, and unavailable handling.

The data layer preserves complete title and agenda text. A later renderer may
show long text with non-blocking horizontal scrolling but must not permanently
truncate the stored model.

Still unresolved:

- calendar/service provider;
- source of the agenda or summary;
- refresh interval and offline cache behavior.

### Shared header

Frozen semantic content:

- current time;
- compact weather condition/icon state;
- temperature;
- M5Stick battery percentage when known;
- compact Wi-Fi/backend connection state.

Still unresolved:

- weather provider;
- location strategy;
- timezone configuration;
- exact visual icon/text treatment, which belongs to the renderer and theme.

## Product principles

### Glanceable first

The most important information should be readable quickly. Dense visual styling may create atmosphere, but should not force the user to decipher the screen.

### Stable behavior across themes

Changing theme must not change what a button does, what data means, or which screen appears.

### A companion, not a miniature desktop

The device should summarize and surface priority information. It should not attempt to reproduce every feature of a calendar app, terminal, phone, or smartwatch.

### Five polished product themes before additional themes

The finished product theme set is NERV-inspired, Ghost HUD-inspired,
Pip-Boy-inspired, Alien / Weyland-Yutani-inspired, and Gundam
cockpit-inspired. Plain remains an internal fallback. Product themes are
completed and hardware-reviewed one at a time rather than presented as finished
while only placeholders exist.

Themed screens show real product data only. Source-inspired motifs and icons
may be decorative, but they must not imply telemetry that the model does not
provide. Unknown values remain explicit. Themes should support both vertical
and horizontal compositions where practical without changing screen meaning.

## Explicit non-goals for the MVP

- reproducing the original Chat Stick chat experience as the main product;
- maintaining separate applications for each visual theme;
- building ten or more themes at once;
- adding complex animated transitions before core stability;
- storing routine network credentials only in source files;
- becoming a general-purpose smartwatch.

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

Agreed semantic content:

- Codex state or activity;
- Codex usage or progress;
- relevant warning or success state.

Not yet frozen:

- the precise metric names;
- whether usage is a percentage, quota, elapsed value, or a combination;
- the backend endpoint and refresh interval;
- whether status refers to a task, session, allowance, or service health.

### Screen B — Meeting / agenda

Agreed semantic content:

- current or next meeting;
- meeting or agenda information.

Not yet frozen:

- exact fields shown;
- source calendar or service;
- whether summaries are generated locally, by the backend, or by an AI provider;
- refresh interval and offline cache behavior.

### Shared header

Agreed semantic content:

- current time;
- current weather information.

Not yet frozen:

- weather source;
- exact weather fields;
- location strategy;
- whether battery or connection icons are added.

## Product principles

### Glanceable first

The most important information should be readable quickly. Dense visual styling may create atmosphere, but should not force the user to decipher the screen.

### Stable behavior across themes

Changing theme must not change what a button does, what data means, or which screen appears.

### A companion, not a miniature desktop

The device should summarize and surface priority information. It should not attempt to reproduce every feature of a calendar app, terminal, phone, or smartwatch.

### Four polished themes before many themes

The MVP quality target is four complete themes rather than many incomplete themes.

## Explicit non-goals for the MVP

- reproducing the original Chat Stick chat experience as the main product;
- maintaining separate applications for each visual theme;
- building ten or more themes at once;
- adding complex animated transitions before core stability;
- storing routine network credentials only in source files;
- becoming a general-purpose smartwatch.

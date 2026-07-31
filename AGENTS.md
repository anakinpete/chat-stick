# Instructions for Codex and Coding Agents

## Read before editing

Before making changes, read:

1. `PROJECT_CONTEXT.md`
2. `docs/PRODUCT_SPEC.md`
3. `docs/UI_SPEC.md`
4. `docs/ARCHITECTURE.md`
5. `docs/DECISIONS.md`
6. `docs/OPEN_QUESTIONS.md`

The original Chat Stick behavior is not the product goal. Do not assume that preserving or extending chat flow is the primary objective.

## Project invariants

Do not violate these rules unless the user explicitly changes the design:

- one firmware, not separate firmware per theme;
- one shared application/data model;
- one shared renderer and screen flow;
- themes alter visuals only;
- two main MVP information screens: Codex/status and Meeting/agenda;
- shared header contains time and weather;
- active theme persists in ESP32 Preferences;
- Wi-Fi and backend settings should be configurable through a browser portal;
- no credentials or passwords in logs, screenshots, commits, or responses.

## Scope discipline

For every task:

- change only the smallest relevant set of files;
- preserve unrelated behavior;
- avoid repository-wide cleanup unless explicitly requested;
- do not rename or move broad directory trees as part of a feature task;
- do not add a new library when an existing dependency or small local implementation is sufficient;
- stop and report a conflict rather than guessing when a decision is listed in `docs/OPEN_QUESTIONS.md`.

## Implementation style

- Prefer small classes or modules with clear ownership.
- Keep networking and data fetching out of theme classes.
- Keep business logic out of drawing code.
- Keep passwords out of serial output and UI output.
- Avoid allocation-heavy work in the render loop.
- Preserve offline boot and backend-unavailable behavior.
- Use ESP32 Preferences for persistent user settings unless a later decision changes this.

## Validation

After code changes:

1. run the narrowest relevant checks;
2. build the `m5stick-s3` PlatformIO environment;
3. report whether the build passed;
4. list every file changed;
5. summarize behavior changed, behavior preserved, and remaining risks.

Do not claim on-device success unless the firmware was actually uploaded and tested on the device.

## Response format for implementation tasks

Keep the final report compact:

- Files changed
- What changed
- Validation performed
- Result
- Any unresolved issue

Do not spend output on a long tutorial unless requested.

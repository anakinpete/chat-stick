# Architecture and Product Decision Log

## ADR-001 — One firmware, multiple themes

**Decision:** Use one firmware and one shared application layer. Themes alter presentation only.

**Reason:** Avoid duplicated logic, reduce maintenance, and make new themes inexpensive to add.

## ADR-002 — Shared two-screen MVP

**Decision:** Keep two main information screens: Codex/status and Meeting/agenda.

**Reason:** This is the agreed product structure and keeps the small display focused.

## ADR-003 — Shared header content

**Decision:** Show time and weather as shared context across the two main screens.

**Reason:** These are persistent glanceable signals and were part of the original renderer concept.

## ADR-004 — Themes do not alter semantics

**Decision:** Themes can change colours, fonts, borders, panel shapes, icons, boot styling, alerts, progress bars, and lightweight animations, but not information meaning or navigation behavior.

**Reason:** Preserve one product and one data model rather than separate themed applications.

## ADR-005 — Persistent theme selection

**Decision:** Save the active theme in ESP32 Preferences.

**Reason:** The user's selection should survive restart.

## ADR-006 — Runtime theme selector gesture

**Decision:** Hold both buttons for approximately two seconds during normal operation to open the theme selector. Button B moves and Button A selects.

**Reason:** Provides theme access without adding another permanent screen or requiring recompilation.

## ADR-007 — Browser-based configuration

**Decision:** Replace routine hardcoded Wi-Fi and backend configuration with an on-device access point and browser form.

**Reason:** Changing networks should not require editing code or reflashing firmware.

## ADR-008 — ESP32 Preferences for MVP settings

**Decision:** Use a central settings abstraction backed by ESP32 Preferences for network, backend, and theme settings.

**Reason:** It is suitable for small persistent values and avoids scattering storage calls throughout the firmware.

## ADR-009 — Five product themes

**Decision:** The finished product set is NERV-inspired, Ghost HUD-inspired,
Pip-Boy-inspired, Alien / Weyland-Yutani-inspired, and Gundam
cockpit-inspired. Plain is an internal fallback; LCARS is removed from the
active plan.

**Reason:** These five distinct identities cover the agreed visual directions
while retaining one shared renderer and data model.

## ADR-010 — Class-based theme direction

**Decision:** Prefer a base theme interface with implementations for each theme, subject to validation against the existing display architecture and device resource use.

**Reason:** Themes may later need small amounts of animation or widget state while retaining a shared contract.

## ADR-011 — Chat is not the primary product flow

**Decision:** Do not treat the original Chat Stick chat flow as the central MVP experience.

**Reason:** The repository is a foundation for a custom status and meeting companion.

## ADR-012 — Plain UI before themed UI

**Decision:** First make one plain functional two-screen UI and freeze its semantic renderer contract before implementing themes.

**Reason:** Prevent repeated rework across five themes while core data and layout are still changing.

## ADR-013 — Codex-first, small-task workflow

**Decision:** Use Codex for substantial implementation, but split work into narrow, testable tasks. Perform only small, obvious, low-risk edits manually.

**Reason:** Speed development, conserve credit, and reduce broad regressions.

## ADR-014 — Repository documentation is durable project memory

**Decision:** Store the product goal, UI rules, architecture, workflow, decisions, and open questions in the repository. Use `AGENTS.md` for standing Codex instructions.

**Reason:** Do not depend on chat history or human memory to preserve essential design context.

## ADR-015 — Defer breadth and animation

**Decision:** Defer random theme on boot, day/night variants, full transitions, extra themes, and optional AI/chat features until after the stable MVP.

**Reason:** Protect core quality and weekend-build scope.

## ADR-016 — Freeze the shared MVP UI data model

**Decision:** Use one theme-independent `CompanionUiModel` containing a shared
header, Codex/status data, meeting/agenda data, and the active primary screen.
The header contains time, weather and temperature, battery percentage, and
compact connection state.

Task progress and Codex allowance remaining are separate optional percentage
values. Unknown allowance remains unknown until a real source is selected.
Meeting data represents the current meeting when in progress, otherwise the
next meeting, and preserves full title and agenda text.

Optional numeric values and timestamps use an explicit known/unknown
representation. Percentage construction rejects values outside `0..100`.
Scrolling, layout, styling, theme selection, network access, and backend
payload parsing remain outside the model.

**Reason:** Freeze semantic meaning before the plain renderer and themes are
built, without prematurely choosing providers or coupling application data to
presentation details.

## ADR-017 — Add a provisional plain companion renderer

**Decision:** Render the shared companion model through a neutral two-screen
renderer on the existing M5Stick canvas when the application is ready. Keep the
legacy renderer for setup, menu, alarm, connection, recording, and error flows.

Until live providers exist, populate the model through one isolated demo
adapter that also accepts real time, battery, Wi-Fi, and backend signals.
Meeting title and agenda use clipped, elapsed-time marquees with no delay loop.

Short Button A toggles primary screens and holding Button B opens the companion
menu. Holding Button A alone does not start legacy push-to-talk in normal
companion mode. Renderer/widget geometry remains provisional until hardware
review.

**Reason:** Make the companion product inspectable on the real display without
coupling presentation to providers or prematurely implementing themes.

## ADR-018 — Use a lightweight theme-style contract

**Decision:** Keep one `CompanionRenderer` and pass it a static `ThemeStyle`
containing presentation-only palette, typography, spacing, border, progress,
and semantic-symbol tokens. Use `SemanticPresentation` for shared human-readable
state labels.

`ThemeManager` centrally maps identifiers, exposes themes, loads the saved
selection through `SettingsStore`, and falls back to Plain for invalid or
unimplemented themes. Plain is the only available theme in this phase; NERV,
Ghost HUD, Pip-Boy, Alien Terminal, and Gundam Cockpit are enumerated but
unavailable.

The normal menu is Back, Codex, Meeting, Theme, and Device. Theme and the A+B
two-second chord open the same manager-backed selector.

**Reason:** Establish theme ownership without virtual-heavy duplicate renderers
or theme branches in application logic, while retaining a readable hardware
fallback.

## ADR-019 — Runtime companion input and safe reset

**Decision:** In normal companion operation, short Button A toggles the primary
screen, hold Button B opens the menu, and hold A+B for about two seconds opens
the theme selector. The chord is consumed before individual actions and input
is suppressed until both buttons are released. Button B moves with wrap,
Button A confirms, and Cancel or menu-back leaves the active theme unchanged.

Unfinished product themes remain visible as non-selectable `[soon]` entries.
Factory reset is removed from the runtime chord and is reachable through the
Device menu with confirmation. Existing reset policy retains Wi-Fi, backend,
and theme settings.

**Reason:** Make theme access predictable, disable legacy voice behavior in the
primary product flow, and prevent accidental settings erasure.

## ADR-020 — Theme visuals use real data only

**Decision:** All themes render the same semantic product data and represent
unknown values honestly. Source-inspired icons and motifs may decorate the UI
but must not imply fake telemetry. Themes should support both vertical and
horizontal compositions where practical.

**Reason:** Preserve trust and product meaning while allowing strong visual
identities.

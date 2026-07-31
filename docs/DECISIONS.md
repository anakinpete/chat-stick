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

## ADR-009 — Four themes for the MVP

**Decision:** Release first with NERV, Ghost HUD, Pip-Boy, and LCARS.

**Reason:** Four polished themes are more valuable than ten incomplete themes.

## ADR-010 — Class-based theme direction

**Decision:** Prefer a base theme interface with implementations for each theme, subject to validation against the existing display architecture and device resource use.

**Reason:** Themes may later need small amounts of animation or widget state while retaining a shared contract.

## ADR-011 — Chat is not the primary product flow

**Decision:** Do not treat the original Chat Stick chat flow as the central MVP experience.

**Reason:** The repository is a foundation for a custom status and meeting companion.

## ADR-012 — Plain UI before themed UI

**Decision:** First make one plain functional two-screen UI and freeze its semantic renderer contract before implementing themes.

**Reason:** Prevent repeated rework across four themes while core data and layout are still changing.

## ADR-013 — Codex-first, small-task workflow

**Decision:** Use Codex for substantial implementation, but split work into narrow, testable tasks. Perform only small, obvious, low-risk edits manually.

**Reason:** Speed development, conserve credit, and reduce broad regressions.

## ADR-014 — Repository documentation is durable project memory

**Decision:** Store the product goal, UI rules, architecture, workflow, decisions, and open questions in the repository. Use `AGENTS.md` for standing Codex instructions.

**Reason:** Do not depend on chat history or human memory to preserve essential design context.

## ADR-015 — Defer breadth and animation

**Decision:** Defer random theme on boot, day/night variants, full transitions, extra themes, and optional AI/chat features until after the stable MVP.

**Reason:** Protect core quality and weekend-build scope.

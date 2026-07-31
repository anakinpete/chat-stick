# M5 Sci-Fi Companion Documentation

This folder is the durable project memory for the M5Stick S3 companion. Copy its contents into the root of the `chat-stick` repository.

## Read first

1. `PROJECT_CONTEXT.md` — the agreed product goal, scope, current state, and next milestone.
2. `AGENTS.md` — standing instructions for Codex and other coding agents.
3. `docs/PRODUCT_SPEC.md` — what the device is for and what the MVP must do.
4. `docs/UI_SPEC.md` — the agreed screens, information hierarchy, controls, and visual themes.
5. `docs/ARCHITECTURE.md` — how application logic, rendering, themes, settings, and networking are separated.

## Supporting documents

- `docs/SETUP_PORTAL_SPEC.md` — browser-based Wi-Fi and backend configuration.
- `docs/DEVELOPMENT_WORKFLOW.md` — Codex-first, credit-conscious development process.
- `docs/CODEX_TASK_TEMPLATE.md` — reusable prompt template for small implementation jobs.
- `docs/ROADMAP.md` — ordered milestones.
- `docs/DECISIONS.md` — durable design and architecture decisions.
- `docs/OPEN_QUESTIONS.md` — unresolved items that must not be silently invented.
- `docs/DOCUMENTATION_AUDIT.md` — what was corrected in this revision.

## Source-of-truth rule

When documents appear to conflict, use this order:

1. The latest explicit user decision.
2. `PROJECT_CONTEXT.md`.
3. `docs/DECISIONS.md`.
4. The relevant specification document.
5. Existing implementation details.

Implementation is not automatically the intended design. The original Chat Stick code is a foundation, not the final product definition.

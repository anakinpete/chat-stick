# Documentation Audit

## Material reviewed

This revision was produced by reading the complete previous documentation pack:

- `PROJECT_CONTEXT.md`
- `README.md`
- `docs/ARCHITECTURE.md`
- `docs/SETUP_PORTAL_SPEC.md`
- `docs/DEVELOPMENT_WORKFLOW.md`
- `docs/ROADMAP.md`
- `docs/DECISIONS.md`

It also reconciles the explicit design statements retained in the current project conversation.

## Important gaps found

### 1. The product function was too vague

The previous pack named “Codex/status” and “Meeting/agenda” but did not clearly state that these, together with time and weather, are the agreed MVP information hierarchy.

**Correction:** Added `PRODUCT_SPEC.md` and `UI_SPEC.md`, and strengthened `PROJECT_CONTEXT.md`.

### 2. Core UI data was incorrectly listed as optional future work

The previous roadmap placed weather, meeting data, and Codex usage under “Potential features,” even though they were already part of the agreed two-screen design.

**Correction:** Moved them into the core information model and implementation phases. AI summaries and chat remain optional later enhancements.

### 3. The original Chat Stick chat flow could be mistaken for the product goal

The previous pack said the repository was a foundation, but did not make the non-chat direction strong enough.

**Correction:** Added an explicit product non-goal and ADR stating that chat is not the primary MVP flow.

### 4. Renderer and theme ownership were ambiguous

The previous architecture said the renderer owns layout while themes own panel geometry, without defining the boundary.

**Correction:** The renderer now owns semantic information order and screen behavior. Themes may style and shape widgets within the shared contract but cannot create separate applications.

### 5. Input architecture was missing

The previous high-level diagram began at application state and omitted button/control handling.

**Correction:** Added input/controllers to the architecture and documented agreed versus unresolved button mappings.

### 6. The normal screen navigation mapping had not actually been agreed

The previous pack implied navigation should be confirmed but did not prevent an implementation from silently choosing a permanent mapping.

**Correction:** Added it to `OPEN_QUESTIONS.md` and marked only the theme-selector mapping as agreed.

### 7. Setup mode and runtime theme selection could conflict

Both may use a two-button gesture in different contexts, but the distinction was not documented.

**Correction:** Runtime two-button hold is agreed for themes; boot-time setup gesture remains explicitly unresolved.

### 8. Codex usage policy was too general

The previous workflow said to use small Codex tasks but lacked a durable agent instruction file, task allocation rules, and a reusable prompt.

**Correction:** Added root `AGENTS.md`, expanded `DEVELOPMENT_WORKFLOW.md`, and added `CODEX_TASK_TEMPLATE.md`.

### 9. Agreed facts and unresolved details were mixed together

This made it easy for a future agent to invent metric fields, calendar sources, or controls.

**Correction:** Added `OPEN_QUESTIONS.md` and consistently marked proposed or unresolved details.

### 10. There was no source-of-truth order

**Correction:** Added a conflict-resolution order to `README.md` so current explicit decisions override stale implementation or older notes.

## Result

The revised pack now preserves:

- the device's actual function;
- the two-screen information model;
- shared time and weather context;
- the four-theme visual direction;
- selector behavior and persistent theme choice;
- browser-based provisioning;
- one-renderer architecture;
- the Codex-first, small-task development method;
- unresolved items that require a future decision.

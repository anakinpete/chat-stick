# Codex Task Template

Copy this template for one small job.

```text
Read these project documents before editing:
- AGENTS.md
- PROJECT_CONTEXT.md
- docs/PRODUCT_SPEC.md
- docs/UI_SPEC.md
- docs/ARCHITECTURE.md
- docs/DECISIONS.md
- docs/OPEN_QUESTIONS.md

Task:
[One clear outcome.]

Current behavior:
[What works now and must be preserved.]

Requirements:
- [Requirement 1]
- [Requirement 2]
- [Requirement 3]

Non-goals:
- [Do not implement adjacent feature]
- [Do not broadly refactor]
- [Do not change unrelated behavior]

Constraints:
- Keep credentials out of logs and commits.
- Reuse existing project dependencies where practical.
- Change the smallest relevant set of files.
- Ask or stop if an unresolved design question blocks the task.

Acceptance criteria:
- [Observable result 1]
- [Observable result 2]
- PlatformIO environment `m5stick-s3` builds successfully.

Before editing:
- Inspect the relevant existing code and summarize the planned file changes briefly.

After editing:
- Run the relevant build/checks.
- List every file changed.
- Summarize what changed and what was deliberately not changed.
- Report build result and any remaining risk.
```

## Next recommended task

```text
Task:
Add a small persistent settings abstraction for Wi-Fi SSID, Wi-Fi password,
backend address, backend port, and active theme.

Requirements:
- Back it with ESP32 Preferences.
- Preserve the current working development configuration as a temporary fallback.
- Centralize all new preference keys in one module.
- Include validation for empty host/SSID and invalid port values.
- Do not add the browser portal in this task.
- Do not change display or chat/session behavior.
- Never log the password.

Acceptance criteria:
- Existing firmware still builds.
- Saved values can be written and read after restart.
- Missing saved values use the temporary development fallback.
- Invalid stored values do not crash startup.
```

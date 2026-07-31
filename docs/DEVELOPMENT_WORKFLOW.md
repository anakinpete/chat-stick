# Development Workflow

## Core policy

Use Codex heavily for substantive code work, while keeping each assignment small enough to review, build, and test immediately.

The goal is not to minimize Codex usage at all costs. The goal is to avoid wasting credit on oversized, ambiguous jobs and repeated repair passes.

## Who should do what

### Give to Codex

- a feature that changes multiple files;
- a new class, service, or subsystem;
- a focused refactor with clear boundaries;
- repetitive theme implementation after the interface is stable;
- tests or validation helpers;
- analysis of an unfamiliar part of the repository;
- a compile failure whose cause is not obvious.

### Do manually

- one-line or very small corrections;
- obvious typo, include, path, or naming fixes;
- running build/upload commands;
- reading logs and checking device behavior;
- reviewing a diff;
- updating documentation after a confirmed decision;
- reverting an unwanted isolated change.

A manual edit should be small enough that its effect is immediately understandable. When uncertain, use Codex with a narrow prompt.

## Required work cycle

1. Choose one milestone from `ROADMAP.md`.
2. Confirm that no blocking item exists in `OPEN_QUESTIONS.md`.
3. Create a narrow task using `CODEX_TASK_TEMPLATE.md`.
4. Tell Codex which project documents to read.
5. Require it to inspect existing code before editing.
6. Limit the requested behavior and files where practical.
7. Review the diff before accepting more changes.
8. Build the firmware.
9. Upload and test on hardware when the task affects device behavior.
10. Fix only tiny, obvious issues manually.
11. Commit the working state.
12. Update status, decisions, or open questions when necessary.

## Task sizing rule

A good Codex job normally has:

- one primary outcome;
- one subsystem;
- explicit acceptance criteria;
- explicit non-goals;
- a build or test command;
- a report of changed files.

Bad task:

```text
Turn Chat Stick into the complete sci-fi companion with all themes,
calendar, weather, setup, AI, and animations.
```

Good sequence:

```text
1. Add a settings abstraction backed by Preferences.
2. Add the setup access point and form using that abstraction.
3. Add setup-mode display states.
4. Define the shared UI data model.
5. Implement a plain two-screen renderer.
6. Add ThemeManager and persistent selection.
7. Add one theme.
```

## Credit-saving practices

- Put stable rules in `AGENTS.md` rather than repeating them in every prompt.
- Ask for implementation and a compact report, not a long tutorial.
- Include exact file paths only after they are verified.
- State what must remain unchanged.
- Separate feature work from cleanup.
- Avoid asking Codex to regenerate files that are already correct.
- Review the first patch before requesting a second pass.
- Use the build output to request one targeted fix instead of a broad retry.
- Complete one theme before asking for the next.

## Safety and source control

Before a substantial Codex edit:

- confirm the repository is under Git;
- commit or stash the current working state;
- keep credentials out of tracked files;
- inspect `git diff` after the change;
- do not accept unrelated formatting or mass file movement.

## Validation expectations

For firmware tasks, Codex should at minimum:

- identify the correct PlatformIO project and environment;
- run the relevant build when tool access permits;
- report the exact command and result;
- distinguish compile success from on-device success.

For this project the known environment name is `m5stick-s3`; verify the working directory before running the command.

## Example workflow for the next milestone

### Task 1

Add a settings model/store for SSID, password, backend host, and backend port. Do not add the web portal yet.

### Task 2

Make startup prefer saved settings while preserving the temporary development fallback.

### Task 3

Add the access point, DNS handling if needed, and browser form.

### Task 4

Add setup status screens and hardware test the complete flow.

This sequence costs more prompts than one giant request, but normally saves credit overall by reducing regressions and repair cycles.

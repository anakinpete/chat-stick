# Architecture

## Architectural objective

Separate device behavior from visual presentation so one stable application can support multiple themes without duplicate logic.

## High-level data flow

```text
Buttons / Timers / Network Events
               |
               v
       Controllers and Services
               |
               v
       Shared Application State
               |
               v
      Screen Flow and Renderer
               |
               v
     Active Theme and UI Widgets
               |
               v
             Display
```

## Layer responsibilities

### Input and controllers

Responsible for:

- button events and hold detection;
- current-screen selection;
- opening and controlling the theme selector;
- setup-mode entry;
- translating raw events into application actions.

Not responsible for drawing theme-specific UI.

### Services

Responsible for:

- Wi-Fi connection management;
- browser setup portal;
- backend communication;
- time and weather updates;
- Codex data updates;
- meeting data updates;
- persistence through a settings store.

Services update shared state; they do not draw screens.

### Shared application state

Contains semantic data needed by the renderer, such as:

- header time and weather;
- Codex status and usage/progress;
- meeting/agenda data;
- connection and error states;
- active screen;
- active theme identifier.

The initial implementation may adapt existing structures rather than replacing them all at once.

### Screen flow and renderer

Responsible for:

- choosing which screen to render;
- preserving the shared information hierarchy;
- mapping application state to reusable widgets;
- providing theme-independent screen behavior;
- invoking visual primitives from the active theme.

The renderer must not perform network requests or store credentials.

### Theme layer

Responsible for visual tokens and visual primitives:

- colours;
- fonts;
- spacing and border treatments;
- panel shapes;
- icons;
- alerts;
- progress indicators;
- boot styling;
- lightweight animation state where necessary.

A theme must not own business data, network clients, or navigation rules.

## Layout ownership clarification

The shared renderer owns **semantic layout and information order**. A theme may vary the geometry of a panel or widget inside an allocated region, but it must not rearrange the product into a different application.

This prevents two common failures:

1. a theme becoming a duplicate screen implementation;
2. the renderer becoming full of `if (theme == ...)` branches.

## Recommended folder direction

Adapt incrementally to the existing repository rather than moving the whole codebase at once.

```text
src/
  core/
    AppState.*
    Settings.*
    ThemeManager.*
    ScreenController.*
  input/
    ButtonController.*
  screens/
    CodexScreen.*
    MeetingScreen.*
    ThemeSelectorScreen.*
    SetupStatusScreen.*
  ui/
    Renderer.*
    Widgets.*
  themes/
    Theme.h
    BaseTheme.*
    NervTheme.*
    GhostTheme.*
    PipBoyTheme.*
    LcarsTheme.*
  networking/
    WifiManager.*
    SetupPortal.*
    BackendClient.*
```

File names may be adjusted to fit the existing code. The separation of responsibilities matters more than these exact names.

## Theme interface direction

A class-based interface remains the preferred direction because themes may need small amounts of visual state.

Illustrative only:

```cpp
class Theme {
public:
    virtual ~Theme() = default;

    virtual ThemeId id() const = 0;
    virtual const char* name() const = 0;

    virtual const ThemePalette& palette() const = 0;
    virtual void drawFrame(Display& display, const Rect& bounds) = 0;
    virtual void drawProgress(Display& display,
                              const Rect& bounds,
                              int value) = 0;
    virtual void drawAlert(Display& display,
                           const Rect& bounds,
                           AlertType type) = 0;
};
```

Do not freeze this exact C++ API before inspecting the existing display abstraction and memory constraints.

## Theme manager

The theme manager should:

- expose the current `ThemeId`;
- provide the active theme implementation;
- enumerate available themes for the selector;
- validate saved theme values;
- save a confirmed selection through the settings store;
- fall back to a known base theme when a value is invalid.

## Persistent settings

Use ESP32 Preferences for user settings, including:

### MVP

- Wi-Fi SSID;
- Wi-Fi password;
- backend address;
- backend port;
- active theme.

### Later

- device name;
- brightness;
- timezone;
- 12/24-hour mode;
- metric/imperial units;
- day/night mode;
- random-theme-on-boot.

Use a settings abstraction rather than scattering direct Preferences calls throughout the code.

## Reliability constraints

- Device startup must not depend on the backend being available.
- A temporary network outage must not erase settings.
- Invalid or missing configuration must lead to a recoverable setup state.
- Render code must remain responsive and non-blocking.
- Frequent drawing paths should avoid unnecessary heap allocation.
- Credentials must never appear in normal logs.
- Refactoring must preserve the known working build after each milestone.

## Data contract freeze point

Before implementing the four themed renderers:

1. define the semantic fields for header, Codex, and Meeting data;
2. implement them in a plain base UI;
3. verify both screens on hardware;
4. freeze the renderer/widget contract for the MVP;
5. then add themes one at a time.

This prevents repeated theme rewrites while the data model is still changing.

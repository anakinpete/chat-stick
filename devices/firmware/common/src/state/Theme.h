#pragma once

#include <stdint.h>

/**
 * @brief Stable visual theme identities.
 *
 * Plain is the safe fallback. Product themes become selectable as their
 * presentation implementations are completed.
 */
enum class ThemeId : uint8_t {
  Plain,
  Nerv,
  GhostHud,
  PipBoy,
  AlienTerminal,
  GundamCockpit,
};

struct ThemePalette {
  uint16_t background = 0x0000;
  uint16_t primary = 0xFFFF;
  uint16_t secondary = 0xBDF7;
  uint16_t muted = 0x7BEF;
  uint16_t info = 0x07FF;
  uint16_t success = 0x07E0;
  uint16_t warning = 0xFFE0;
  uint16_t error = 0xF800;
};

enum class ThemeFontRole : uint8_t {
  CompactReadable,
};

struct ThemeTypography {
  ThemeFontRole bodyFont = ThemeFontRole::CompactReadable;
  uint8_t textScale = 1;
  uint8_t lineHeightPx = 16;
};

struct ThemeSpacing {
  uint8_t edgeInsetPx = 4;
  uint8_t smallGapPx = 4;
  uint8_t mediumGapPx = 8;
};

enum class ThemePanelShape : uint8_t {
  Flat,
  Rounded,
  Angled,
};

struct ThemeBorders {
  bool showSectionDividers = true;
  uint8_t dividerThicknessPx = 1;
  ThemePanelShape panelShape = ThemePanelShape::Flat;
};

struct ThemeProgressStyle {
  uint8_t barHeightPx = 4;
  bool outlined = true;
  uint8_t segmentCount = 1;
};

enum class ThemeAlertTreatment : uint8_t {
  TextOnly,
  Outline,
  Filled,
};

struct ThemeAlertStyle {
  ThemeAlertTreatment treatment = ThemeAlertTreatment::TextOnly;
};

enum class ThemeIndicatorMode : uint8_t {
  Text,
  SymbolAndText,
};

enum class ThemeOrientation : uint8_t {
  Landscape,
  Portrait,
};

enum class ThemeComposition : uint8_t {
  Standard,
  Stacked,
};

enum class ThemeMark : uint8_t {
  None,
  Wordmark,
};

/**
 * @brief Reusable presentation primitives selected by a theme.
 */
struct ThemePrimitives {
  ThemeOrientation orientation = ThemeOrientation::Landscape;
  ThemeComposition composition = ThemeComposition::Standard;
  ThemeMark mark = ThemeMark::None;
  uint8_t cornerCutPx = 0;
  uint8_t sideRailWidthPx = 0;
  uint8_t primaryTextScale = 1;
};

/**
 * @brief ASCII-safe semantic symbols for the current display font.
 */
struct ThemeSymbols {
  ThemeIndicatorMode indicatorMode = ThemeIndicatorMode::Text;
  const char *battery = "BAT";

  const char *weatherUnknown = "?";
  const char *weatherClear = "*";
  const char *weatherPartlyCloudy = "~";
  const char *weatherCloudy = "=";
  const char *weatherRain = "R";
  const char *weatherStorm = "!";
  const char *weatherSnow = "+";
  const char *weatherFog = "-";
  const char *weatherWind = ">";

  const char *connectionOffline = "";
  const char *connectionWiFi = "";
  const char *connectionConnecting = "";
  const char *connectionOnline = "";

  const char *activityIdle = "";
  const char *activityWorking = "";
  const char *activityWaiting = "";
  const char *activityComplete = "";
  const char *activityError = "";
};

/**
 * @brief Presentation-only contract consumed by the shared renderer.
 */
struct ThemeStyle {
  ThemeId id = ThemeId::Plain;
  const char *identifier = "plain";
  const char *displayName = "Plain";
  ThemePalette palette;
  ThemeTypography typography;
  ThemeSpacing spacing;
  ThemeBorders borders;
  ThemeProgressStyle progress;
  ThemeAlertStyle alerts;
  ThemeSymbols symbols;
  ThemePrimitives primitives;
};

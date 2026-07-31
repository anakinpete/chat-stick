#pragma once

#include "Theme.h"
#include <Arduino.h>

class SettingsStore;

/**
 * @brief Central theme identity, availability, persistence, and style owner.
 */
class ThemeManager {
public:
  void init(const SettingsStore &settings);

  ThemeId activeTheme() const { return _activeTheme; }
  const ThemeStyle &activeStyle() const;

  int availableThemeCount() const;
  ThemeId availableThemeAt(int index) const;
  const char *displayName(ThemeId id) const;

  bool activate(ThemeId id, SettingsStore &settings);

  static ThemeId resolveAvailableIdentifier(const String &identifier);
  static bool tryParseIdentifier(const String &identifier, ThemeId &id);
  static bool isAvailable(ThemeId id);
  static const char *identifier(ThemeId id);

private:
  ThemeId _activeTheme = ThemeId::Plain;
};

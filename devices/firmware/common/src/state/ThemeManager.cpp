#include "ThemeManager.h"

#include "BaseTheme.h"
#include "services/SettingsStore.h"

namespace {
struct ThemeDescriptor {
  ThemeId id;
  const char *identifier;
  const char *displayName;
  bool available;
};

constexpr ThemeDescriptor kThemes[] = {
    {ThemeId::Plain, "plain", "Plain", true},
    {ThemeId::Nerv, "nerv", "NERV", false},
    {ThemeId::GhostHud, "ghost", "Ghost HUD", false},
    {ThemeId::PipBoy, "pip-boy", "Pip-Boy", false},
    {ThemeId::AlienTerminal, "alien", "Alien Terminal", false},
    {ThemeId::GundamCockpit, "gundam", "Gundam Cockpit", false},
};
constexpr int kThemeCount = sizeof(kThemes) / sizeof(kThemes[0]);

const ThemeDescriptor *descriptor(ThemeId id) {
  for (int i = 0; i < kThemeCount; ++i) {
    if (kThemes[i].id == id) {
      return &kThemes[i];
    }
  }
  return nullptr;
}
} // namespace

void ThemeManager::init(const SettingsStore &settings) {
  _activeTheme = resolveAvailableIdentifier(settings.activeTheme());
}

const ThemeStyle &ThemeManager::activeStyle() const {
  // Plain is the only implemented style. Unavailable ids can never become
  // active, and this fallback keeps rendering safe if that invariant changes.
  return baseThemeStyle();
}

int ThemeManager::availableThemeCount() const {
  int count = 0;
  for (int i = 0; i < kThemeCount; ++i) {
    if (kThemes[i].available) {
      ++count;
    }
  }
  return count;
}

ThemeId ThemeManager::availableThemeAt(int index) const {
  if (index < 0) {
    return ThemeId::Plain;
  }
  for (int i = 0; i < kThemeCount; ++i) {
    if (!kThemes[i].available) {
      continue;
    }
    if (index-- == 0) {
      return kThemes[i].id;
    }
  }
  return ThemeId::Plain;
}

int ThemeManager::themeCount() const { return kThemeCount; }

ThemeId ThemeManager::themeAt(int index) const {
  return index >= 0 && index < kThemeCount ? kThemes[index].id
                                           : ThemeId::Plain;
}

const char *ThemeManager::displayName(ThemeId id) const {
  const ThemeDescriptor *entry = descriptor(id);
  return entry ? entry->displayName : "Plain";
}

bool ThemeManager::activate(ThemeId id, SettingsStore &settings) {
  if (!isAvailable(id) ||
      !settings.saveActiveTheme(String(identifier(id)))) {
    return false;
  }
  _activeTheme = id;
  return true;
}

ThemeId ThemeManager::resolveAvailableIdentifier(const String &identifier) {
  ThemeId parsed = ThemeId::Plain;
  return tryParseIdentifier(identifier, parsed) && isAvailable(parsed)
             ? parsed
             : ThemeId::Plain;
}

bool ThemeManager::tryParseIdentifier(const String &identifier,
                                      ThemeId &id) {
  for (int i = 0; i < kThemeCount; ++i) {
    if (identifier == kThemes[i].identifier) {
      id = kThemes[i].id;
      return true;
    }
  }
  return false;
}

bool ThemeManager::isAvailable(ThemeId id) {
  const ThemeDescriptor *entry = descriptor(id);
  return entry && entry->available;
}

const char *ThemeManager::identifier(ThemeId id) {
  const ThemeDescriptor *entry = descriptor(id);
  return entry ? entry->identifier : "plain";
}

#pragma once

#include "CompanionUiModel.h"
#include "Theme.h"
#include <stdint.h>

/**
 * @brief Shared human-readable labels and semantic theme lookups.
 */
class SemanticPresentation {
public:
  static const char *connectionLabel(ConnectionState state);
  static const char *connectionSymbol(const ThemeStyle &theme,
                                      ConnectionState state);
  static uint16_t connectionColor(const ThemeStyle &theme,
                                  ConnectionState state);

  static const char *weatherSymbol(const ThemeStyle &theme,
                                   WeatherCondition condition);

  static const char *activityLabel(CodexActivityState state);
  static const char *activitySymbol(const ThemeStyle &theme,
                                    CodexActivityState state);
  static const char *availabilityLabel(DataAvailability state);
  static uint16_t availabilityColor(const ThemeStyle &theme,
                                    DataAvailability state);
  static uint16_t severityColor(const ThemeStyle &theme, UiSeverity severity);

  static const char *meetingStateLabel(MeetingState state);
  static const char *locationTypeLabel(MeetingLocationType type);
};

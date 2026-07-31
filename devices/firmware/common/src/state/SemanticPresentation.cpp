#include "SemanticPresentation.h"

const char *SemanticPresentation::connectionLabel(ConnectionState state) {
  switch (state) {
  case ConnectionState::Offline:
    return "OFFLINE";
  case ConnectionState::WiFiOnly:
    return "WIFI";
  case ConnectionState::BackendConnecting:
    return "SYNC";
  case ConnectionState::Online:
    return "ONLINE";
  }
  return "OFFLINE";
}

const char *
SemanticPresentation::connectionSymbol(const ThemeStyle &theme,
                                       ConnectionState state) {
  switch (state) {
  case ConnectionState::Offline:
    return theme.symbols.connectionOffline;
  case ConnectionState::WiFiOnly:
    return theme.symbols.connectionWiFi;
  case ConnectionState::BackendConnecting:
    return theme.symbols.connectionConnecting;
  case ConnectionState::Online:
    return theme.symbols.connectionOnline;
  }
  return "";
}

uint16_t
SemanticPresentation::connectionColor(const ThemeStyle &theme,
                                      ConnectionState state) {
  switch (state) {
  case ConnectionState::Online:
    return theme.palette.success;
  case ConnectionState::BackendConnecting:
  case ConnectionState::WiFiOnly:
    return theme.palette.warning;
  case ConnectionState::Offline:
  default:
    return theme.palette.error;
  }
}

const char *
SemanticPresentation::weatherSymbol(const ThemeStyle &theme,
                                    WeatherCondition condition) {
  switch (condition) {
  case WeatherCondition::Clear:
    return theme.symbols.weatherClear;
  case WeatherCondition::PartlyCloudy:
    return theme.symbols.weatherPartlyCloudy;
  case WeatherCondition::Cloudy:
    return theme.symbols.weatherCloudy;
  case WeatherCondition::Rain:
    return theme.symbols.weatherRain;
  case WeatherCondition::Storm:
    return theme.symbols.weatherStorm;
  case WeatherCondition::Snow:
    return theme.symbols.weatherSnow;
  case WeatherCondition::Fog:
    return theme.symbols.weatherFog;
  case WeatherCondition::Wind:
    return theme.symbols.weatherWind;
  case WeatherCondition::Unknown:
  default:
    return theme.symbols.weatherUnknown;
  }
}

const char *
SemanticPresentation::activityLabel(CodexActivityState state) {
  switch (state) {
  case CodexActivityState::Idle:
    return "IDLE";
  case CodexActivityState::Working:
    return "WORKING";
  case CodexActivityState::Done:
    return "DONE";
  case CodexActivityState::Cancelled:
    return "STOPPED";
  case CodexActivityState::Stale:
    return "STALE";
  case CodexActivityState::Offline:
    return "OFFLINE";
    return "ERROR";
  case CodexActivityState::Unavailable:
  default:
    return "UNAVAILABLE";
  }
}

const char *
SemanticPresentation::activitySymbol(const ThemeStyle &theme,
                                     CodexActivityState state) {
  switch (state) {
  case CodexActivityState::Idle:
    return theme.symbols.activityIdle;
  case CodexActivityState::Working:
    return theme.symbols.activityWorking;
  case CodexActivityState::Done:
    return theme.symbols.activityComplete;
  case CodexActivityState::Cancelled:
    return theme.symbols.activityWaiting;
  case CodexActivityState::Stale:
    return theme.symbols.activityWaiting;
  case CodexActivityState::Offline:
  case CodexActivityState::Unavailable:
  default:
    return theme.symbols.activityError;
  }
}

const char *
SemanticPresentation::availabilityLabel(DataAvailability state) {
  switch (state) {
  case DataAvailability::Unknown:
    return "UNKNOWN";
  case DataAvailability::Loading:
    return "LOADING";
  case DataAvailability::Available:
    return "AVAILABLE";
  case DataAvailability::Stale:
    return "STALE";
  case DataAvailability::Unavailable:
    return "UNAVAILABLE";
  case DataAvailability::Error:
    return "ERROR";
  }
  return "UNKNOWN";
}

uint16_t
SemanticPresentation::availabilityColor(const ThemeStyle &theme,
                                        DataAvailability state) {
  switch (state) {
  case DataAvailability::Loading:
    return theme.palette.info;
  case DataAvailability::Available:
    return theme.palette.success;
  case DataAvailability::Stale:
    return theme.palette.warning;
  case DataAvailability::Error:
    return theme.palette.error;
  case DataAvailability::Unavailable:
  case DataAvailability::Unknown:
  default:
    return theme.palette.muted;
  }
}

uint16_t SemanticPresentation::severityColor(const ThemeStyle &theme,
                                             UiSeverity severity) {
  switch (severity) {
  case UiSeverity::Info:
    return theme.palette.info;
  case UiSeverity::Warning:
    return theme.palette.warning;
  case UiSeverity::Success:
    return theme.palette.success;
  case UiSeverity::Error:
    return theme.palette.error;
  case UiSeverity::Normal:
  default:
    return theme.palette.primary;
  }
}

const char *SemanticPresentation::meetingStateLabel(MeetingState state) {
  switch (state) {
  case MeetingState::None:
    return "NO MEETING";
  case MeetingState::Upcoming:
    return "NEXT";
  case MeetingState::InProgress:
    return "IN PROGRESS";
  case MeetingState::Finished:
    return "FINISHED";
  case MeetingState::Unavailable:
  default:
    return "UNAVAILABLE";
  }
}

const char *
SemanticPresentation::locationTypeLabel(MeetingLocationType type) {
  switch (type) {
  case MeetingLocationType::Physical:
    return "LOCATION";
  case MeetingLocationType::VideoCall:
    return "VIDEO";
  case MeetingLocationType::PhoneCall:
    return "PHONE";
  case MeetingLocationType::Hybrid:
    return "HYBRID";
  case MeetingLocationType::Other:
    return "LOCATION";
  case MeetingLocationType::Unknown:
  default:
    return "WHERE";
  }
}

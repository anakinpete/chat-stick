#pragma once

#include <Arduino.h>
#include <stdint.h>

/**
 * @brief Explicitly optional semantic value.
 *
 * A value is meaningful only when known is true. This avoids sentinel values
 * that could be mistaken for real telemetry.
 */
template <typename T> struct OptionalValue {
  bool known = false;
  T value{};

  static OptionalValue unknown() { return OptionalValue{}; }

  static OptionalValue from(const T &knownValue) {
    OptionalValue result;
    result.known = true;
    result.value = knownValue;
    return result;
  }
};

/**
 * @brief Optional percentage constrained to the inclusive range 0..100.
 */
struct PercentageValue {
  bool known = false;
  uint8_t value = 0;

  static PercentageValue unknown() { return PercentageValue{}; }

  static PercentageValue from(int percentage) {
    PercentageValue result;
    if (percentage >= 0 && percentage <= 100) {
      result.known = true;
      result.value = static_cast<uint8_t>(percentage);
    }
    return result;
  }
};

/**
 * @brief Freshness and usability of a semantic data group.
 */
enum class DataAvailability {
  Unknown,
  Loading,
  Available,
  Stale,
  Unavailable,
  Error,
};

/**
 * @brief Device connectivity summarized independently from presentation.
 */
enum class ConnectionState {
  Offline,
  WiFiOnly,
  BackendConnecting,
  Online,
};

/**
 * @brief Semantic emphasis requested by the data, not a visual style.
 */
enum class UiSeverity {
  Normal,
  Info,
  Warning,
  Success,
  Error,
};

/**
 * @brief Provider-independent compact weather condition.
 */
enum class WeatherCondition {
  Unknown,
  Clear,
  PartlyCloudy,
  Cloudy,
  Rain,
  Storm,
  Snow,
  Fog,
  Wind,
};

enum class TemperatureUnit {
  Celsius,
  Fahrenheit,
};

/**
 * @brief Weather values shown in the shared header.
 */
struct WeatherData {
  DataAvailability availability = DataAvailability::Unknown;
  WeatherCondition condition = WeatherCondition::Unknown;
  OptionalValue<float> temperature;
  TemperatureUnit temperatureUnit = TemperatureUnit::Celsius;
};

/**
 * @brief Shared context shown above either primary screen.
 */
struct HeaderData {
  /// Current wall-clock instant as Unix seconds; formatting is renderer-owned.
  OptionalValue<int64_t> currentTimeUnixSeconds;

  WeatherData weather;

  /// M5Stick battery level when the board reports a valid 0..100 value.
  PercentageValue batteryPercentage;

  ConnectionState connection = ConnectionState::Offline;
};

enum class CodexActivityState {
  Idle,
  Working,
  Done,
  Cancelled,
  Stale,
  Offline,
  Unavailable,
};

/**
 * @brief Codex task and allowance values for the status screen.
 */
struct CodexStatusData {
  DataAvailability availability = DataAvailability::Unknown;
  CodexActivityState activity = CodexActivityState::Unavailable;
  UiSeverity severity = UiSeverity::Normal;

  /// Locally derived duration; valid only when known is true.
  OptionalValue<uint32_t> elapsedSeconds;

  /// Remaining Codex usage or credit, independent of task progress.
  DataAvailability allowanceAvailability = DataAvailability::Unknown;
  PercentageValue allowanceRemainingPercentage;
  String allowanceResetText;
  OptionalValue<int64_t> allowanceResetUnixSeconds;

  OptionalValue<int64_t> lastUpdateUnixSeconds;
};

enum class MeetingState {
  None,
  Upcoming,
  InProgress,
  Finished,
  Unavailable,
};

enum class MeetingLocationType {
  Unknown,
  Physical,
  VideoCall,
  PhoneCall,
  Hybrid,
  Other,
};

/**
 * @brief Current meeting, or otherwise the next meeting.
 *
 * Full text is retained here. Truncation and non-blocking horizontal scrolling
 * belong to the later renderer/widget layer.
 */
struct MeetingData {
  DataAvailability availability = DataAvailability::Unknown;
  MeetingState state = MeetingState::Unavailable;

  String title;
  OptionalValue<int64_t> startTimeUnixSeconds;
  OptionalValue<uint32_t> secondsUntilStart;
  OptionalValue<uint32_t> secondsRemaining;

  MeetingLocationType locationType = MeetingLocationType::Unknown;
  String location;
  String agendaSummary;
};

enum class PrimaryScreen {
  Codex,
  Meeting,
};

/**
 * @brief Theme-independent snapshot consumed by the future companion renderer.
 */
struct CompanionUiModel {
  HeaderData header;
  CodexStatusData codex;
  MeetingData meeting;
  PrimaryScreen activeScreen = PrimaryScreen::Codex;
};

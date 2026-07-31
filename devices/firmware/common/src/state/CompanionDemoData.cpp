#include "CompanionDemoData.h"

namespace {
constexpr int64_t kMinimumValidEpoch = 1704067200;
constexpr int64_t kDemoMeetingLeadSeconds = 25 * 60;
constexpr int64_t kDemoMeetingDurationSeconds = 30 * 60;

ConnectionState connectionState(const CompanionRuntimeSignals &signals) {
  if (!signals.wifiConnected) {
    return ConnectionState::Offline;
  }
  if (signals.backendConnected) {
    return ConnectionState::Online;
  }
  return signals.backendConnecting ? ConnectionState::BackendConnecting
                                   : ConnectionState::WiFiOnly;
}
} // namespace

const CompanionUiModel &
CompanionDemoData::update(const CompanionRuntimeSignals &signals,
                          PrimaryScreen activeScreen) {
  if (!_initialized) {
    // DEVELOPMENT DEMO DATA: replace with provider adapters later.
    _model.header.weather.availability = DataAvailability::Available;
    _model.header.weather.condition = WeatherCondition::PartlyCloudy;
    _model.header.weather.temperature = OptionalValue<float>::from(18.0f);
    _model.header.weather.temperatureUnit = TemperatureUnit::Celsius;

    _model.meeting.title =
        "M5 Sci-Fi Companion architecture and hardware review";
    _model.meeting.locationType = MeetingLocationType::VideoCall;
    _model.meeting.location = "Video call";
    _model.meeting.agendaSummary =
        "Review the plain layout, controls, marquee timing, and data contract";
    _initialized = true;
  }

  _model.activeScreen = activeScreen;

  if (signals.currentTimeUnixSeconds >= kMinimumValidEpoch) {
    _model.header.currentTimeUnixSeconds =
        OptionalValue<int64_t>::from(signals.currentTimeUnixSeconds);
  } else {
    _model.header.currentTimeUnixSeconds = OptionalValue<int64_t>::unknown();
  }
  _model.header.batteryPercentage =
      PercentageValue::from(signals.batteryPercentage);
  _model.header.connection = connectionState(signals);
  _model.codex.allowanceAvailability = signals.allowanceAvailability;
  _model.codex.allowanceRemainingPercentage =
      signals.allowanceRemainingPercentage;
  _model.codex.allowanceResetUnixSeconds =
      signals.allowanceResetUnixSeconds;
  _model.codex.allowanceResetText = "";

  _model.codex.availability = signals.activityAvailability;
  _model.codex.activity = signals.activity;
  _model.codex.elapsedSeconds = signals.activityElapsedSeconds;
  switch (signals.activity) {
  case CodexActivityState::Working:
    _model.codex.severity = UiSeverity::Warning;
    break;
  case CodexActivityState::Done:
    _model.codex.severity = UiSeverity::Success;
    break;
  case CodexActivityState::Cancelled:
    _model.codex.severity = UiSeverity::Warning;
    break;
  case CodexActivityState::Stale:
    _model.codex.severity = UiSeverity::Warning;
    break;
  case CodexActivityState::Offline:
  case CodexActivityState::Unavailable:
    _model.codex.severity = UiSeverity::Error;
    break;
  case CodexActivityState::Idle:
  default:
    _model.codex.severity = UiSeverity::Normal;
    break;
  }
  if (signals.activityUpdatedUnixSeconds.known) {
    _model.codex.lastUpdateUnixSeconds = signals.activityUpdatedUnixSeconds;
  } else if (signals.allowanceUpdatedUnixSeconds.known) {
    _model.codex.lastUpdateUnixSeconds =
        signals.allowanceUpdatedUnixSeconds;
  } else if (_model.header.currentTimeUnixSeconds.known) {
    _model.codex.lastUpdateUnixSeconds =
        _model.header.currentTimeUnixSeconds;
  } else {
    _model.codex.lastUpdateUnixSeconds = OptionalValue<int64_t>::unknown();
  }

  if (_model.header.currentTimeUnixSeconds.known) {
    const int64_t now = _model.header.currentTimeUnixSeconds.value;
    if (_meetingStartUnixSeconds == 0) {
      _meetingStartUnixSeconds = now + kDemoMeetingLeadSeconds;
    }
    _model.meeting.startTimeUnixSeconds =
        OptionalValue<int64_t>::from(_meetingStartUnixSeconds);
    if (now < _meetingStartUnixSeconds) {
      _model.meeting.state = MeetingState::Upcoming;
      _model.meeting.secondsUntilStart = OptionalValue<uint32_t>::from(
          static_cast<uint32_t>(_meetingStartUnixSeconds - now));
      _model.meeting.secondsRemaining = OptionalValue<uint32_t>::unknown();
    } else if (now < _meetingStartUnixSeconds +
                         kDemoMeetingDurationSeconds) {
      _model.meeting.state = MeetingState::InProgress;
      _model.meeting.secondsUntilStart = OptionalValue<uint32_t>::unknown();
      _model.meeting.secondsRemaining = OptionalValue<uint32_t>::from(
          static_cast<uint32_t>(_meetingStartUnixSeconds +
                                kDemoMeetingDurationSeconds - now));
    } else {
      _model.meeting.state = MeetingState::Finished;
      _model.meeting.secondsUntilStart = OptionalValue<uint32_t>::unknown();
      _model.meeting.secondsRemaining = OptionalValue<uint32_t>::unknown();
    }
  } else {
    _model.meeting.state = MeetingState::Upcoming;
    _model.meeting.startTimeUnixSeconds = OptionalValue<int64_t>::unknown();
    _model.meeting.secondsUntilStart =
        OptionalValue<uint32_t>::from(
            static_cast<uint32_t>(kDemoMeetingLeadSeconds));
    _model.meeting.secondsRemaining = OptionalValue<uint32_t>::unknown();
  }

  if (signals.backendConnected) {
    _model.meeting.availability = DataAvailability::Available;
  } else if (signals.wifiConnected) {
    _model.meeting.availability = DataAvailability::Loading;
  } else {
    _model.meeting.availability = DataAvailability::Stale;
  }

  return _model;
}

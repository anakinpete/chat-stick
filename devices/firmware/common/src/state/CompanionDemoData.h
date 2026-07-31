#pragma once

#include "CompanionUiModel.h"
#include <stdint.h>

/**
 * @brief Real device signals currently available to the companion UI.
 */
struct CompanionRuntimeSignals {
  int64_t currentTimeUnixSeconds = 0;
  int batteryPercentage = -1;
  bool wifiConnected = false;
  bool backendConnected = false;
  bool backendConnecting = false;
  CodexActivityState activity = CodexActivityState::Unavailable;
  DataAvailability activityAvailability = DataAvailability::Unavailable;
  OptionalValue<uint32_t> activityElapsedSeconds =
      OptionalValue<uint32_t>::unknown();
  OptionalValue<int64_t> activityUpdatedUnixSeconds =
      OptionalValue<int64_t>::unknown();
  DataAvailability allowanceAvailability = DataAvailability::Unknown;
  PercentageValue allowanceRemainingPercentage = PercentageValue::unknown();
  OptionalValue<int64_t> allowanceResetUnixSeconds =
      OptionalValue<int64_t>::unknown();
  OptionalValue<int64_t> allowanceUpdatedUnixSeconds =
      OptionalValue<int64_t>::unknown();
};

/**
 * @brief Development adapter used until live companion providers are added.
 *
 * Hardware, Codex activity, allowance, and connection values come from
 * CompanionRuntimeSignals. Weather and meeting content remain isolated demo
 * data until their providers are implemented.
 */
class CompanionDemoData {
public:
  const CompanionUiModel &update(const CompanionRuntimeSignals &signals,
                                 PrimaryScreen activeScreen);

private:
  CompanionUiModel _model;
  int64_t _meetingStartUnixSeconds = 0;
  bool _initialized = false;
};

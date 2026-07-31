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
};

/**
 * @brief Development adapter used until live companion providers are added.
 *
 * Hardware and connection values come from CompanionRuntimeSignals. Weather,
 * Codex, and meeting content below is deliberately isolated demo data.
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

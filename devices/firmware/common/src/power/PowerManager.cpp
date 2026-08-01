#include "PowerManager.h"

#include "Config.h"
#include "hal/BoardPower.h"
#include "power/PowerPolicy.h"
#include <stdarg.h>

const char *powerStateName(PowerState state) {
  switch (state) {
  case PowerState::Active:
    return "Active";
  case PowerState::Dimmed:
    return "Dimmed";
  case PowerState::ScreenOff:
    return "ScreenOff";
  case PowerState::Waking:
    return "Waking";
  case PowerState::LightSleep:
    return "LightSleep";
  case PowerState::PowerOff:
    return "PowerOff";
  default:
    return "Unknown";
  }
}

PowerManager::PowerManager()
    : _state(PowerState::Active), _lastActivityMs(0),
      _savedBrightness(DEFAULT_BRIGHTNESS),
      _timeouts({IDLE_DIM_MS, IDLE_SCREEN_OFF_MS, IDLE_LIGHT_SLEEP_MS,
                 IDLE_POWER_OFF_MS}) {}

void PowerManager::begin() {
  const unsigned long now = millis();
  _state = PowerState::Active;
  _bootStartedMs = now;
  _lastActivityMs = now;
  _begun = true;
  _externalPowerConnected = true;
  _powerSourceKnown = false;
  _powerSourceCandidate = true;
  _powerSourceCandidateSamples = 0;
  _lastPowerSourcePollMs = now;
  _powerSourcePollStarted = false;
}

void PowerManager::update(bool inactivityEnabled) {
  if (!_begun) {
    begin();
    return;
  }

  const unsigned long now = millis();
  updatePowerSource(now);

  if (_state == PowerState::Waking || _state == PowerState::PowerOff ||
      !_powerSourceKnown) {
    return;
  }
  if (!inactivityEnabled) {
    return;
  }

  const PowerSourcePolicy source =
      !_powerSourceKnown
          ? PowerSourcePolicy::Unknown
          : _externalPowerConnected ? PowerSourcePolicy::External
                                    : PowerSourcePolicy::Battery;
  const IdlePolicyTarget policy = selectSafeIdlePolicyTarget(
      source, elapsedPolicyMs(now, _bootStartedMs),
      elapsedPolicyMs(now, _lastActivityMs), _timeouts.dimMs,
      _timeouts.screenOffMs, _timeouts.powerOffMs,
      BOOT_POWER_OFF_GUARD_MS);
  PowerState target = PowerState::Active;
  switch (policy) {
  case IdlePolicyTarget::Dimmed:
    target = PowerState::Dimmed;
    break;
  case IdlePolicyTarget::ScreenOff:
    target = PowerState::ScreenOff;
    break;
  case IdlePolicyTarget::PowerOff:
    target = PowerState::PowerOff;
    break;
  case IdlePolicyTarget::Active:
  default:
    target = PowerState::Active;
    break;
  }

  if (target == PowerState::Active && _state != PowerState::Active) {
    restoreActive();
  } else if (target > _state) {
    transitionTo(target);
  }
}

void PowerManager::registerActivity() {
  noteMeaningfulActivity();
}

void PowerManager::noteMeaningfulActivity() {
  if (!_begun) {
    begin();
    return;
  }
  _lastActivityMs = millis();

  if (isInterruptible()) {
    restoreActive();
  }
}

void PowerManager::setTimeouts(const PowerTimeouts &timeouts) {
  const NormalizedPolicyTimeouts normalized = normalizePolicyTimeouts(
      timeouts.dimMs, timeouts.screenOffMs, timeouts.powerOffMs,
      IDLE_DIM_MS, IDLE_SCREEN_OFF_MS, IDLE_POWER_OFF_MS);
  _timeouts.dimMs = normalized.dimMs;
  _timeouts.screenOffMs = normalized.screenOffMs;
  _timeouts.powerOffMs = normalized.powerOffMs;
  // The field remains in the runtime contract, but normal inactivity has no
  // sleep stage. Align it with final power-off so it cannot imply an earlier
  // transition or break the ordered timeout report.
  _timeouts.lightSleepMs = _timeouts.powerOffMs;
  logServer("updated timeouts dim=%lu screen=%lu sleep=%lu off=%lu",
            _timeouts.dimMs, _timeouts.screenOffMs, _timeouts.lightSleepMs,
            _timeouts.powerOffMs);
}

void PowerManager::beginWaking() {
  if (!isInterruptible()) {
    return;
  }

  const PowerState previous = _state;
  _state = PowerState::Waking;
  _lastActivityMs = millis();

  logClient("%s -> Waking", powerStateName(previous));

  applyCpuFrequency(CPU_ACTIVE_MHZ);

  if (_brightnessCallback) {
    _brightnessCallback(_savedBrightness);
  }
}

void PowerManager::finishWaking() {
  if (_state != PowerState::Waking) {
    return;
  }

  _state = PowerState::Active;
  _lastActivityMs = millis();
  applyCpuFrequency(CPU_ACTIVE_MHZ);
  logClient("Waking -> Active");
}

void PowerManager::restoreActive() {
  if (_state == PowerState::Active || _state == PowerState::Waking) {
    return;
  }

  logClient("%s -> Active", powerStateName(_state));
  _state = PowerState::Active;
  applyCpuFrequency(CPU_ACTIVE_MHZ);

  if (_brightnessCallback) {
    _brightnessCallback(_savedBrightness);
  }
}

unsigned long PowerManager::getIdleTime() const {
  return millis() - _lastActivityMs;
}

void PowerManager::updatePowerSource(unsigned long now) {
  if (!Board::capabilities().usbPowerStatus) {
    if (!_powerSourceKnown || !_externalPowerConnected) {
      _powerSourceKnown = true;
      _externalPowerConnected = true;
      noteMeaningfulActivity();
      logClient("external power status unavailable; using always-on policy");
    }
    return;
  }

  if (_powerSourcePollStarted &&
      now - _lastPowerSourcePollMs < POWER_SOURCE_POLL_MS) {
    return;
  }
  _powerSourcePollStarted = true;
  _lastPowerSourcePollMs = now;

  const bool sample = Board::usbConnected();
  if (_powerSourceCandidateSamples == 0 || sample != _powerSourceCandidate) {
    _powerSourceCandidate = sample;
    _powerSourceCandidateSamples = 1;
    return;
  }
  if (!isPowerSourceDebounced(_powerSourceCandidateSamples,
                              POWER_SOURCE_CONFIRM_SAMPLES)) {
    ++_powerSourceCandidateSamples;
  }
  if (!isPowerSourceDebounced(_powerSourceCandidateSamples,
                              POWER_SOURCE_CONFIRM_SAMPLES)) {
    return;
  }
  if (_powerSourceKnown && _externalPowerConnected == sample) {
    return;
  }

  const bool wasKnown = _powerSourceKnown;
  _powerSourceKnown = true;
  _externalPowerConnected = sample;
  noteMeaningfulActivity();
  logClient("power source %s%s", sample ? "USB" : "battery",
            wasKnown ? " (changed)" : "");
}

void PowerManager::applyCpuFrequency(int mhz) {
  if (_cpuFrequencyCallback) {
    _cpuFrequencyCallback(mhz);
  }
}

void PowerManager::transitionTo(PowerState newState) {
  if (newState == _state) {
    return;
  }

  const PowerState oldState = _state;
  _state = newState;

  logClient("%s -> %s", powerStateName(oldState), powerStateName(newState));

  switch (newState) {
  case PowerState::Active:
    applyCpuFrequency(CPU_ACTIVE_MHZ);
    if (_brightnessCallback) {
      _brightnessCallback(_savedBrightness);
    }
    break;

  case PowerState::Dimmed:
    applyCpuFrequency(CPU_ACTIVE_MHZ);
    if (_brightnessCallback) {
      _brightnessCallback(BRIGHTNESS_DIM);
    }
    break;

  case PowerState::ScreenOff:
    applyCpuFrequency(CPU_IDLE_MHZ);
    Board::setAudioAmpEnabled(false);
    if (_brightnessCallback) {
      _brightnessCallback(BRIGHTNESS_OFF);
    }
    break;

  case PowerState::LightSleep:
    enterLightSleep();
    break;

  case PowerState::PowerOff:
    applyCpuFrequency(CPU_IDLE_MHZ);
    Board::setAudioAmpEnabled(false);
    if (_brightnessCallback) {
      _brightnessCallback(BRIGHTNESS_OFF);
    }
    if (_wifiCallback) {
      _wifiCallback(false);
    }
    if (_powerOffCallback) {
      _powerOffCallback();
    }
    break;

  case PowerState::Waking:
    break;
  }
}

void PowerManager::enterLightSleep() {
  logClient("entering light sleep");

  if (_brightnessCallback) {
    _brightnessCallback(BRIGHTNESS_OFF);
  }
  Board::setAudioAmpEnabled(false);
  if (_wifiCallback) {
    _wifiCallback(false);
  }

  while (true) {
    const LightSleepWakeReason reason =
        Board::enterLightSleep(LIGHT_SLEEP_WAKE_INTERVAL_MS);
    if (reason == LightSleepWakeReason::Button) {
      logClient("light sleep wake: button");
      if (_wifiCallback) {
        _wifiCallback(true);
      }
      _state = PowerState::Waking;
      _lastActivityMs = millis();
      if (_brightnessCallback) {
        _brightnessCallback(_savedBrightness);
      }
      return;
    }

    if (reason == LightSleepWakeReason::Timer &&
        getIdleTime() >= _timeouts.powerOffMs && !_externalPowerConnected) {
      transitionTo(PowerState::PowerOff);
      return;
    }
  }
}

void PowerManager::logClient(const char *fmt, ...) const {
  char message[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);

  if (_logCallback) {
    _logCallback('C', "Power", message);
  } else {
    Serial.printf("[Power] %s\n", message);
  }
}

void PowerManager::logServer(const char *fmt, ...) const {
  char message[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);

  if (_logCallback) {
    _logCallback('S', "Power", message);
  } else {
    Serial.printf("[Power] %s\n", message);
  }
}

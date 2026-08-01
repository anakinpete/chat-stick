#pragma once

#include <stdint.h>

enum class PowerSourcePolicy : uint8_t {
  Unknown,
  External,
  Battery,
};

/**
 * @brief Pure policy result for normal inactivity on the current power source.
 */
enum class IdlePolicyTarget : uint8_t {
  Active,
  Dimmed,
  ScreenOff,
  PowerOff,
};

/**
 * @brief Select the normal inactivity stage without performing hardware I/O.
 */
constexpr IdlePolicyTarget selectIdlePolicyTarget(
    bool externalPowerConnected, unsigned long idleMs,
    unsigned long dimMs, unsigned long screenOffMs,
    unsigned long powerOffMs) {
  return externalPowerConnected
             ? IdlePolicyTarget::Active
             : idleMs >= powerOffMs
                   ? IdlePolicyTarget::PowerOff
                   : idleMs >= screenOffMs
                         ? IdlePolicyTarget::ScreenOff
                         : idleMs >= dimMs ? IdlePolicyTarget::Dimmed
                                           : IdlePolicyTarget::Active;
}

/**
 * @brief Wrap-safe elapsed milliseconds for Arduino's 32-bit millis clock.
 */
constexpr uint32_t elapsedPolicyMs(uint32_t nowMs, uint32_t sinceMs) {
  return nowMs - sinceMs;
}

/**
 * @brief Apply source certainty and the boot power-off guard to idle policy.
 */
constexpr IdlePolicyTarget selectSafeIdlePolicyTarget(
    PowerSourcePolicy source, uint32_t bootAgeMs, uint32_t idleAgeMs,
    uint32_t dimMs, uint32_t screenOffMs, uint32_t powerOffMs,
    uint32_t bootPowerOffGuardMs) {
  return source != PowerSourcePolicy::Battery
             ? IdlePolicyTarget::Active
             : selectIdlePolicyTarget(false, idleAgeMs, dimMs, screenOffMs,
                                      powerOffMs) == IdlePolicyTarget::PowerOff &&
                       bootAgeMs < bootPowerOffGuardMs
                   ? IdlePolicyTarget::ScreenOff
                   : selectIdlePolicyTarget(false, idleAgeMs, dimMs,
                                            screenOffMs, powerOffMs);
}

/**
 * @brief Whether enough identical samples exist to accept a power source.
 */
constexpr bool isPowerSourceDebounced(uint8_t sampleCount,
                                      uint8_t requiredSamples) {
  return requiredSamples > 0 && sampleCount >= requiredSamples;
}

struct NormalizedPolicyTimeouts {
  uint32_t dimMs;
  uint32_t screenOffMs;
  uint32_t powerOffMs;
};

constexpr uint32_t policyMax(uint32_t lhs, uint32_t rhs) {
  return lhs > rhs ? lhs : rhs;
}

constexpr uint32_t policyAddSaturated(uint32_t value, uint32_t increment) {
  return value > UINT32_MAX - increment ? UINT32_MAX : value + increment;
}

constexpr uint32_t normalizedPolicyDim(uint32_t requestedDimMs,
                                       uint32_t defaultDimMs) {
  return policyMax(requestedDimMs, defaultDimMs);
}

constexpr uint32_t normalizedPolicyScreenOff(
    uint32_t requestedDimMs, uint32_t requestedScreenOffMs,
    uint32_t defaultDimMs, uint32_t defaultScreenOffMs) {
  return policyMax(
      policyMax(requestedScreenOffMs, defaultScreenOffMs),
      policyAddSaturated(normalizedPolicyDim(requestedDimMs, defaultDimMs),
                         1000U));
}

constexpr uint32_t normalizedPolicyPowerOff(
    uint32_t requestedDimMs, uint32_t requestedScreenOffMs,
    uint32_t requestedPowerOffMs, uint32_t defaultDimMs,
    uint32_t defaultScreenOffMs, uint32_t defaultPowerOffMs) {
  return policyMax(
      policyMax(requestedPowerOffMs, defaultPowerOffMs),
      policyAddSaturated(
          normalizedPolicyScreenOff(requestedDimMs, requestedScreenOffMs,
                                    defaultDimMs, defaultScreenOffMs),
          1000U));
}

/**
 * @brief Reject zero, short, unordered, and overflowing runtime timeouts.
 */
constexpr NormalizedPolicyTimeouts normalizePolicyTimeouts(
    uint32_t requestedDimMs, uint32_t requestedScreenOffMs,
    uint32_t requestedPowerOffMs, uint32_t defaultDimMs,
    uint32_t defaultScreenOffMs, uint32_t defaultPowerOffMs) {
  return {normalizedPolicyDim(requestedDimMs, defaultDimMs),
          normalizedPolicyScreenOff(requestedDimMs, requestedScreenOffMs,
                                    defaultDimMs, defaultScreenOffMs),
          normalizedPolicyPowerOff(
              requestedDimMs, requestedScreenOffMs, requestedPowerOffMs,
              defaultDimMs, defaultScreenOffMs, defaultPowerOffMs)};
}

/**
 * @brief True only after an initialized semantic state actually changes.
 */
constexpr bool isMeaningfulStateTransition(bool initialized,
                                           uint8_t previousState,
                                           uint8_t currentState) {
  return initialized && previousState != currentState;
}

// Compile-time policy coverage keeps the timing boundaries and the
// state-change reset rule testable without a board-specific test harness.
static_assert(selectIdlePolicyTarget(true, 24UL * 60UL * 60UL * 1000UL,
                                    120000UL, 300000UL, 1800000UL) ==
                  IdlePolicyTarget::Active,
              "USB power must inhibit every inactivity stage");
static_assert(selectIdlePolicyTarget(false, 119999UL, 120000UL, 300000UL,
                                    1800000UL) == IdlePolicyTarget::Active,
              "battery must remain active before two minutes");
static_assert(selectIdlePolicyTarget(false, 120000UL, 120000UL, 300000UL,
                                    1800000UL) == IdlePolicyTarget::Dimmed,
              "battery must dim at two minutes");
static_assert(selectIdlePolicyTarget(false, 300000UL, 120000UL, 300000UL,
                                    1800000UL) == IdlePolicyTarget::ScreenOff,
              "battery screen must turn off at five minutes");
static_assert(selectIdlePolicyTarget(false, 1800000UL, 120000UL, 300000UL,
                                    1800000UL) == IdlePolicyTarget::PowerOff,
              "battery must power off at thirty minutes");
static_assert(!isMeaningfulStateTransition(false, 0, 1),
              "initial state observation must not fake activity");
static_assert(!isMeaningfulStateTransition(true, 1, 1),
              "identical state polls must not reset inactivity");
static_assert(isMeaningfulStateTransition(true, 1, 2),
              "semantic state changes must reset inactivity");
static_assert(selectSafeIdlePolicyTarget(
                  PowerSourcePolicy::Battery, 0U, 0U, 120000U, 300000U,
                  1800000U, 1800000U) == IdlePolicyTarget::Active,
              "boot at millis zero must remain active");
static_assert(selectSafeIdlePolicyTarget(
                  PowerSourcePolicy::Unknown, 1800000U, 1800000U, 120000U,
                  300000U, 1800000U, 1800000U) == IdlePolicyTarget::Active,
              "unknown source must use the always-on policy");
static_assert(!isPowerSourceDebounced(1, 3),
              "first source sample must not unlock battery policy");
static_assert(!isPowerSourceDebounced(2, 3),
              "second source sample must not unlock battery policy");
static_assert(isPowerSourceDebounced(3, 3),
              "third matching source sample establishes certainty");
static_assert(selectSafeIdlePolicyTarget(
                  PowerSourcePolicy::External, UINT32_MAX, UINT32_MAX,
                  120000U, 300000U, 1800000U, 1800000U) ==
                  IdlePolicyTarget::Active,
              "confirmed USB must never power off from inactivity");
static_assert(elapsedPolicyMs(5000U, 5000U) == 0U,
              "confirmed battery must begin with zero idle age");
static_assert(selectSafeIdlePolicyTarget(
                  PowerSourcePolicy::Battery, 1799999U, 1799999U, 120000U,
                  300000U, 1800000U, 1800000U) ==
                  IdlePolicyTarget::ScreenOff,
              "battery must not power off before a full thirty minutes");
static_assert(selectSafeIdlePolicyTarget(
                  PowerSourcePolicy::Battery, 1800000U, 1800000U, 120000U,
                  300000U, 1800000U, 1800000U) ==
                  IdlePolicyTarget::PowerOff,
              "battery may power off at a full thirty minutes");
static_assert(normalizePolicyTimeouts(0U, 0U, 0U, 120000U, 300000U,
                                     1800000U)
                      .powerOffMs == 1800000U,
              "zero legacy timeouts must normalize to safe defaults");
static_assert(normalizePolicyTimeouts(600000U, 1U, 2U, 120000U, 300000U,
                                     1800000U)
                          .screenOffMs >
                      normalizePolicyTimeouts(600000U, 1U, 2U, 120000U,
                                              300000U, 1800000U)
                          .dimMs,
              "unordered runtime values must be made safe and ordered");
static_assert(elapsedPolicyMs(0x00000100U, 0xFFFFFF00U) == 0x00000200U,
              "millis wraparound must produce a small elapsed age");
static_assert(elapsedPolicyMs(9000U, 9000U) == 0U,
              "USB-to-battery transition must start a fresh timer");
static_assert(elapsedPolicyMs(0U, 0U) == 0U,
              "reboot must start with a fresh inactivity baseline");

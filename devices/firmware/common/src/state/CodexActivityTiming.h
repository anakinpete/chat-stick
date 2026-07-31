#pragma once

#include <stdint.h>

struct CodexActivityElapsed {
  bool known;
  uint32_t seconds;

  constexpr CodexActivityElapsed(bool knownValue = false,
                                 uint32_t secondsValue = 0)
      : known(knownValue), seconds(secondsValue) {}
};

constexpr CodexActivityElapsed calculateCodexActivityElapsed(
    bool startedKnown, int64_t startedAt, bool completedKnown,
    int64_t completedAt, bool allowLiveElapsed, int64_t currentTime,
    int64_t minimumTrustedEpoch = 1704067200,
    int64_t futureToleranceSeconds = 5) {
  return !startedKnown
             ? CodexActivityElapsed()
             : completedKnown
                   ? (completedAt < startedAt ||
                              completedAt - startedAt > 0xFFFFFFFFLL ||
                              (currentTime >= minimumTrustedEpoch &&
                               completedAt >
                                   currentTime + futureToleranceSeconds)
                          ? CodexActivityElapsed()
                          : CodexActivityElapsed(
                                true, static_cast<uint32_t>(completedAt -
                                                            startedAt)))
                   : (!allowLiveElapsed ||
                              currentTime < minimumTrustedEpoch ||
                              currentTime < startedAt ||
                              currentTime - startedAt > 0xFFFFFFFFLL
                          ? CodexActivityElapsed()
                          : CodexActivityElapsed(
                                true, static_cast<uint32_t>(currentTime -
                                                            startedAt)));
}

// Compile-time regression cases for running, terminal freeze, and safe
// state-only fallbacks. These run as part of every firmware build.
static_assert(calculateCodexActivityElapsed(true, 1704067200, false, 0, true,
                                            1704067260)
                  .seconds == 60);
static_assert(calculateCodexActivityElapsed(true, 1704067200, true,
                                            1704067260, false, 1704067300)
                  .seconds == 60);
static_assert(calculateCodexActivityElapsed(true, 1704067200, true,
                                            1704067260, false, 1704069999)
                  .seconds == 60);
static_assert(!calculateCodexActivityElapsed(true, 1704067200, false, 0,
                                             false, 1704067300)
                   .known);
static_assert(!calculateCodexActivityElapsed(true, 1704067260, true,
                                             1704067200, false, 1704067300)
                   .known);

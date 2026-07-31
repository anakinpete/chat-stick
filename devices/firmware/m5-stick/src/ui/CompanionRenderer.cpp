#include "CompanionRenderer.h"

#include "../Config.h"
#include "state/SemanticPresentation.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

namespace {
void formatTime(const OptionalValue<int64_t> &value, char *buffer,
                size_t bufferSize) {
  if (!value.known) {
    snprintf(buffer, bufferSize, "--:--");
    return;
  }
  const time_t instant = static_cast<time_t>(value.value);
  struct tm local;
  if (!localtime_r(&instant, &local)) {
    snprintf(buffer, bufferSize, "--:--");
    return;
  }
  snprintf(buffer, bufferSize, "%02d:%02d", local.tm_hour, local.tm_min);
}

bool formatResetDateTime(const OptionalValue<int64_t> &value,
                         char *dateBuffer, size_t dateBufferSize,
                         char *timeBuffer, size_t timeBufferSize) {
  if (!value.known) return false;

  const time_t instant = static_cast<time_t>(value.value);
  struct tm local;
  if (!localtime_r(&instant, &local) || local.tm_mon < 0 ||
      local.tm_mon > 11) {
    return false;
  }

  static constexpr const char *months[] = {
      "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
      "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  snprintf(dateBuffer, dateBufferSize, "%02d %s", local.tm_mday,
           months[local.tm_mon]);
  snprintf(timeBuffer, timeBufferSize, "%02d:%02d", local.tm_hour,
           local.tm_min);
  return true;
}

void formatPercentage(const PercentageValue &value, char *buffer,
                      size_t bufferSize) {
  if (!value.known) {
    snprintf(buffer, bufferSize, "--");
    return;
  }
  snprintf(buffer, bufferSize, "%u%%", static_cast<unsigned>(value.value));
}

void formatActivityElapsed(const OptionalValue<uint32_t> &value, char *buffer,
                           size_t bufferSize) {
  if (!value.known) {
    buffer[0] = '\0';
    return;
  }
  const uint32_t seconds = value.value;
  if (seconds >= 3600) {
    snprintf(buffer, bufferSize, "ELAPSED %luH %02luM",
             static_cast<unsigned long>(seconds / 3600),
             static_cast<unsigned long>((seconds % 3600) / 60));
  } else {
    snprintf(buffer, bufferSize, "ELAPSED %luM %02luS",
             static_cast<unsigned long>(seconds / 60),
             static_cast<unsigned long>(seconds % 60));
  }
}

void formatDuration(uint32_t seconds, const char *suffix, char *buffer,
                    size_t bufferSize) {
  if (seconds >= 3600) {
    snprintf(buffer, bufferSize, "%luh%02lum %s",
             static_cast<unsigned long>(seconds / 3600),
             static_cast<unsigned long>((seconds % 3600) / 60), suffix);
  } else if (seconds >= 60) {
    snprintf(buffer, bufferSize, "%lum %s",
             static_cast<unsigned long>(seconds / 60), suffix);
  } else {
    snprintf(buffer, bufferSize, "%lus %s",
             static_cast<unsigned long>(seconds), suffix);
  }
}

int measureTextWidth(M5Canvas &canvas, bool canvasReady, const char *text) {
  return canvasReady ? canvas.textWidth(text) : M5.Display.textWidth(text);
}

} // namespace

void CompanionRenderer::render(M5Canvas &canvas, bool canvasReady,
                               const CompanionUiModel &model,
                               const ThemeStyle &theme,
                               unsigned long nowMs) {
  if (model.activeScreen == PrimaryScreen::Codex) {
    _lastCodexFrameMs = nowMs;
  }
  if (!_hasRenderedScreen || _lastScreen != model.activeScreen ||
      _lastTheme != theme.id) {
    _meetingTitleMarquee.reset();
    _meetingAgendaMarquee.reset();
    _lastScreen = model.activeScreen;
    _lastTheme = theme.id;
    _hasRenderedScreen = true;
  }

  if (theme.primitives.composition == ThemeComposition::Stacked) {
    drawSideRail(canvas, canvasReady,
                 displayHeight(canvas, canvasReady), theme);
    drawStackedHeader(canvas, canvasReady, model.header, theme);
    if (model.activeScreen == PrimaryScreen::Codex) {
      drawStackedCodexScreen(canvas, canvasReady, model.codex, theme, nowMs);
    } else {
      drawStackedMeetingScreen(canvas, canvasReady, model, theme, nowMs);
    }
    drawStackedFooter(canvas, canvasReady, theme);
    return;
  }

  drawHeader(canvas, canvasReady, model.header, theme);
  if (model.activeScreen == PrimaryScreen::Codex) {
    drawCodexScreen(canvas, canvasReady, model.codex, theme, nowMs);
  } else {
    drawMeetingScreen(canvas, canvasReady, model, theme, nowMs);
  }
  drawFooter(canvas, canvasReady, theme);
}

bool CompanionRenderer::needsFrame(unsigned long nowMs) const {
  if (!_hasRenderedScreen) {
    return false;
  }
  if (_lastScreen == PrimaryScreen::Codex) {
    return nowMs - _lastCodexFrameMs >= 1000;
  }
  return _meetingTitleMarquee.needsFrame(nowMs) ||
         _meetingAgendaMarquee.needsFrame(nowMs);
}

void CompanionRenderer::reset() {
  _hasRenderedScreen = false;
  _lastCodexFrameMs = 0;
  _lastTheme = ThemeId::Plain;
  _meetingTitleMarquee.reset();
  _meetingAgendaMarquee.reset();
}

void CompanionRenderer::drawStackedHeader(M5Canvas &canvas, bool canvasReady,
                                           const HeaderData &header,
                                           const ThemeStyle &theme) const {
  const int width = displayWidth(canvas, canvasReady);
  const int edge = theme.spacing.edgeInsetPx;
  drawThemeMark(canvas, canvasReady, edge, 0, theme);

  char timeText[8];
  formatTime(header.currentTimeUnixSeconds, timeText, sizeof(timeText));
  const int timeX = width - edge -
                    measureTextWidth(canvas, canvasReady, timeText);
  drawText(canvas, canvasReady, timeX, 0, timeText,
           theme.palette.primary);

  char weatherText[14];
  if ((header.weather.availability == DataAvailability::Available ||
       header.weather.availability == DataAvailability::Stale) &&
      header.weather.temperature.known) {
    const char unit = header.weather.temperatureUnit ==
                              TemperatureUnit::Fahrenheit
                          ? 'F'
                          : 'C';
    snprintf(weatherText, sizeof(weatherText), "%s%.0f%c",
             SemanticPresentation::weatherSymbol(theme,
                                                   header.weather.condition),
             header.weather.temperature.value, unit);
  } else {
    snprintf(weatherText, sizeof(weatherText), "%s--",
             theme.symbols.weatherUnknown);
  }

  char batteryText[8];
  formatPercentage(header.batteryPercentage, batteryText,
                   sizeof(batteryText));

  const char *connection =
      SemanticPresentation::connectionLabel(header.connection);
  const int rowEdge = 1;
  const int weatherWidth =
      measureTextWidth(canvas, canvasReady, weatherText);
  const int batteryTextWidth =
      measureTextWidth(canvas, canvasReady, batteryText);
  const int batteryGroupWidth = 11 + batteryTextWidth;
  const int connectionWidth =
      measureTextWidth(canvas, canvasReady, connection);
  const int connectionX = width - rowEdge - connectionWidth;
  const int batteryMinX = rowEdge + weatherWidth + 1;
  const int batteryMaxX = connectionX - batteryGroupWidth - 1;
  const int batteryX =
      min(batteryMaxX, max(batteryMinX, (width - batteryGroupWidth) / 2));

  drawText(canvas, canvasReady, rowEdge, 18, weatherText,
           theme.palette.info);
  drawBatteryIcon(canvas, canvasReady, batteryX, 21, theme.palette.info);
  drawText(canvas, canvasReady, batteryX + 11, 18, batteryText,
           theme.palette.primary);
  drawText(canvas, canvasReady, connectionX, 18, connection,
           SemanticPresentation::connectionColor(theme, header.connection));
  drawHorizontalLine(canvas, canvasReady, 37, theme.palette.secondary,
                     theme.borders.dividerThicknessPx);
}

void CompanionRenderer::drawStackedCodexScreen(
    M5Canvas &canvas, bool canvasReady, const CodexStatusData &codex,
    const ThemeStyle &theme, unsigned long /*nowMs*/) {
  const int contentX = 6;
  const int contentWidth = displayWidth(canvas, canvasReady) - contentX - 4;
  const char *stateText = SemanticPresentation::activityLabel(codex.activity);
  const uint16_t stateColor =
      SemanticPresentation::severityColor(theme, codex.severity);

  drawAngledPanel(canvas, canvasReady, contentX, 43, contentWidth, 35,
                  stateColor, theme);
  uint8_t stateScale = theme.primitives.primaryTextScale;
  const int stateBaseWidth =
      measureTextWidth(canvas, canvasReady, stateText);
  if (stateBaseWidth * stateScale > contentWidth - 10) {
    stateScale = 1;
  }
  const int stateWidth = stateBaseWidth * stateScale;
  drawScaledText(canvas, canvasReady,
                 contentX + max(5, (contentWidth - stateWidth) / 2),
                 stateScale > 1 ? 44 : 52, stateText, stateColor, stateScale,
                 theme);

  char elapsed[24];
  formatActivityElapsed(codex.elapsedSeconds, elapsed, sizeof(elapsed));
  if (elapsed[0]) {
    const int elapsedWidth = measureTextWidth(canvas, canvasReady, elapsed);
    drawText(canvas, canvasReady,
             contentX + max(0, (contentWidth - elapsedWidth) / 2), 91,
             elapsed, theme.palette.primary);
  }

  char allowanceValue[8];
  const bool allowanceKnown =
      (codex.allowanceAvailability == DataAvailability::Available ||
       codex.allowanceAvailability == DataAvailability::Stale) &&
      codex.allowanceRemainingPercentage.known;
  if (allowanceKnown) {
    formatPercentage(codex.allowanceRemainingPercentage, allowanceValue,
                     sizeof(allowanceValue));
  } else if (codex.allowanceAvailability == DataAvailability::Loading) {
    snprintf(allowanceValue, sizeof(allowanceValue), "LOADING");
  } else {
    snprintf(allowanceValue, sizeof(allowanceValue), "N/A");
  }
  const uint16_t allowanceColor =
      codex.allowanceAvailability == DataAvailability::Stale
          ? theme.palette.warning
          : codex.allowanceAvailability == DataAvailability::Error
                ? theme.palette.error
                : codex.allowanceAvailability == DataAvailability::Loading
                      ? theme.palette.info
                      : theme.palette.secondary;
  const char *allowanceLabel = "ALLOW";
  const int allowanceLabelWidth =
      measureTextWidth(canvas, canvasReady, allowanceLabel);
  const int allowanceValueWidth =
      measureTextWidth(canvas, canvasReady, allowanceValue);
  const int allowanceGap =
      min(8, max(1, contentWidth - allowanceLabelWidth - allowanceValueWidth));
  const int allowanceValueX =
      contentX + allowanceLabelWidth + allowanceGap;
  drawText(canvas, canvasReady, contentX, 141, allowanceLabel,
           theme.palette.primary);
  drawText(canvas, canvasReady, allowanceValueX, 141, allowanceValue,
           allowanceColor);
  if (allowanceKnown) {
    drawSegmentedProgress(canvas, canvasReady, contentX, 158, contentWidth,
                          codex.allowanceRemainingPercentage,
                          theme.palette.secondary, theme);
  }

  if (allowanceKnown && !codex.allowanceResetText.isEmpty()) {
    drawText(canvas, canvasReady, contentX, 176, "RST",
             theme.palette.muted);
    const int resetValueX =
        contentX + measureTextWidth(canvas, canvasReady, "RST") + 8;
    drawClippedText(canvas, canvasReady, resetValueX, 176,
                    displayWidth(canvas, canvasReady) - resetValueX - 1,
                    codex.allowanceResetText, 0,
                    theme.palette.info);
  } else if (allowanceKnown && codex.allowanceResetUnixSeconds.known) {
    char resetDate[8];
    char resetLocalTime[8];
    if (formatResetDateTime(codex.allowanceResetUnixSeconds, resetDate,
                            sizeof(resetDate), resetLocalTime,
                            sizeof(resetLocalTime))) {
      drawText(canvas, canvasReady, contentX, 176, "RST",
               theme.palette.muted);
      const int resetValueX =
          contentX + measureTextWidth(canvas, canvasReady, "RST") + 8;
      char resetValue[16];
      snprintf(resetValue, sizeof(resetValue), "%s %s", resetDate,
               resetLocalTime);
      drawText(canvas, canvasReady, resetValueX, 176, resetValue,
               theme.palette.info);
    }
  }

  char updated[8];
  formatTime(codex.lastUpdateUnixSeconds, updated, sizeof(updated));
  drawText(canvas, canvasReady, contentX, 198, "UPD", theme.palette.muted);
  const int updatedValueX =
      contentX + measureTextWidth(canvas, canvasReady, "UPD") + 8;
  drawText(canvas, canvasReady, updatedValueX, 198, updated,
           theme.palette.info);
}

void CompanionRenderer::drawStackedMeetingScreen(
    M5Canvas &canvas, bool canvasReady, const CompanionUiModel &model,
    const ThemeStyle &theme, unsigned long nowMs) {
  const MeetingData &meeting = model.meeting;
  const int contentX = 6;
  const int contentWidth = displayWidth(canvas, canvasReady) - contentX - 4;
  const bool available = meeting.availability == DataAvailability::Available;
  const char *stateText = available
                              ? SemanticPresentation::meetingStateLabel(
                                    meeting.state)
                              : SemanticPresentation::availabilityLabel(
                                    meeting.availability);
  if (available && meeting.state == MeetingState::InProgress) {
    stateText = "NOW";
  }
  const uint16_t stateColor =
      available ? theme.palette.secondary
                : SemanticPresentation::availabilityColor(
                      theme, meeting.availability);
  drawAngledPanel(canvas, canvasReady, contentX, 53, contentWidth, 38,
                  stateColor, theme);
  const int stateWidth = static_cast<int>(strlen(stateText)) * 16;
  const uint8_t stateScale = stateWidth <= contentWidth - 10 ? 2 : 1;
  drawScaledText(canvas, canvasReady,
                 contentX + max(5, (contentWidth -
                                    static_cast<int>(strlen(stateText)) * 8 *
                                        stateScale) /
                                       2),
                 stateScale > 1 ? 56 : 64, stateText, stateColor, stateScale,
                 theme);

  if (available && meeting.state == MeetingState::None) {
    drawText(canvas, canvasReady, contentX, 110, "CALENDAR CLEAR",
             theme.palette.primary);
    drawText(canvas, canvasReady, contentX, 131, "No meeting",
             theme.palette.secondary);
    _meetingTitleMarquee.reset();
    _meetingAgendaMarquee.reset();
    return;
  }

  if (!available && meeting.title.isEmpty()) {
    drawText(canvas, canvasReady, contentX, 110,
             meeting.availability == DataAvailability::Loading
                 ? "LOADING MEETINGS"
                 : "MEETING DATA N/A",
             stateColor);
    _meetingTitleMarquee.reset();
    _meetingAgendaMarquee.reset();
    return;
  }

  char startText[8];
  formatTime(meeting.startTimeUnixSeconds, startText, sizeof(startText));
  drawScaledText(canvas, canvasReady, contentX, 94, startText,
                 theme.palette.primary, 2, theme);

  char timing[24] = "--";
  if (meeting.state == MeetingState::InProgress &&
      meeting.secondsRemaining.known) {
    formatDuration(meeting.secondsRemaining.value, "left", timing,
                   sizeof(timing));
  } else if (meeting.secondsUntilStart.known) {
    formatDuration(meeting.secondsUntilStart.value, "to go", timing,
                   sizeof(timing));
  }
  drawText(canvas, canvasReady, contentX, 125, timing,
           theme.palette.info);

  drawText(canvas, canvasReady, contentX, 141, "MEETING",
           theme.palette.muted);
  _meetingTitleMarquee.configure(meeting.title, contentWidth, nowMs);
  drawClippedText(canvas, canvasReady, contentX, 156, contentWidth,
                  meeting.title, _meetingTitleMarquee.offsetPixels(nowMs),
                  theme.palette.primary);
  _meetingTitleMarquee.noteRendered(nowMs);

  const char *locationLabel =
      SemanticPresentation::locationTypeLabel(meeting.locationType);
  drawText(canvas, canvasReady, contentX, 174, locationLabel,
           theme.palette.muted);
  const int locationX =
      contentX + static_cast<int>(strlen(locationLabel)) * 8 + 8;
  drawClippedText(canvas, canvasReady, locationX, 174,
                  max(0, contentX + contentWidth - locationX),
                  meeting.location.isEmpty() ? String("--")
                                             : meeting.location,
                  0, theme.palette.info);

  drawText(canvas, canvasReady, contentX, 191, "AGENDA",
           theme.palette.muted);
  const String agenda =
      meeting.agendaSummary.isEmpty() ? String("--") : meeting.agendaSummary;
  _meetingAgendaMarquee.configure(agenda, contentWidth, nowMs);
  drawClippedText(canvas, canvasReady, contentX, 206, contentWidth, agenda,
                  _meetingAgendaMarquee.offsetPixels(nowMs),
                  theme.palette.primary);
  _meetingAgendaMarquee.noteRendered(nowMs);
}

void CompanionRenderer::drawStackedFooter(M5Canvas &canvas, bool canvasReady,
                                           const ThemeStyle &theme) const {
  const int y = displayHeight(canvas, canvasReady) - 15;
  const char *leftLabel = "A:VIEW";
  const char *rightLabel = "B:HOLD";
  const int edge = 6;
  const int rightX = displayWidth(canvas, canvasReady) - edge -
                     measureTextWidth(canvas, canvasReady, rightLabel);
  drawHorizontalLine(canvas, canvasReady, y - 2, theme.palette.muted, 1);
  drawText(canvas, canvasReady, edge, y, leftLabel, theme.palette.muted);
  drawText(canvas, canvasReady, rightX, y, rightLabel, theme.palette.muted);
}

void CompanionRenderer::drawHeader(M5Canvas &canvas, bool canvasReady,
                                   const HeaderData &header,
                                   const ThemeStyle &theme) const {
  const int edge = theme.spacing.edgeInsetPx;
  char timeText[8];
  formatTime(header.currentTimeUnixSeconds, timeText, sizeof(timeText));
  drawText(canvas, canvasReady, edge, 0, timeText, theme.palette.primary);

  char weatherText[14];
  if (header.weather.availability == DataAvailability::Available ||
      header.weather.availability == DataAvailability::Stale) {
    if (header.weather.temperature.known) {
      const char unit = header.weather.temperatureUnit ==
                                TemperatureUnit::Fahrenheit
                            ? 'F'
                            : 'C';
      snprintf(weatherText, sizeof(weatherText), "%s %.0f%c",
               SemanticPresentation::weatherSymbol(theme,
                                                     header.weather.condition),
               header.weather.temperature.value, unit);
    } else {
      snprintf(weatherText, sizeof(weatherText), "%s --",
               SemanticPresentation::weatherSymbol(theme,
                                                     header.weather.condition));
    }
  } else {
    snprintf(weatherText, sizeof(weatherText), "%s --",
             theme.symbols.weatherUnknown);
  }
  drawText(canvas, canvasReady, 52, 0, weatherText, theme.palette.secondary);

  char batteryText[12];
  if (header.batteryPercentage.known) {
    snprintf(batteryText, sizeof(batteryText), "%s %u%%",
             theme.symbols.battery,
             static_cast<unsigned>(header.batteryPercentage.value));
  } else {
    snprintf(batteryText, sizeof(batteryText), "%s --",
             theme.symbols.battery);
  }
  drawText(canvas, canvasReady, 108, 0, batteryText, theme.palette.secondary);

  const char *connectionSymbol =
      SemanticPresentation::connectionSymbol(theme, header.connection);
  char connectionText[14];
  if (theme.symbols.indicatorMode == ThemeIndicatorMode::SymbolAndText &&
      connectionSymbol && connectionSymbol[0] != '\0') {
    snprintf(connectionText, sizeof(connectionText), "%s %s", connectionSymbol,
             SemanticPresentation::connectionLabel(header.connection));
  } else {
    snprintf(connectionText, sizeof(connectionText), "%s",
             SemanticPresentation::connectionLabel(header.connection));
  }
  drawText(canvas, canvasReady, 180, 0, connectionText,
           SemanticPresentation::connectionColor(theme, header.connection));
  if (theme.borders.showSectionDividers) {
    drawHorizontalLine(canvas, canvasReady, 16, theme.palette.muted,
                       theme.borders.dividerThicknessPx);
  }
}

void CompanionRenderer::drawCodexScreen(M5Canvas &canvas, bool canvasReady,
                                        const CodexStatusData &codex,
                                        const ThemeStyle &theme,
                                        unsigned long /*nowMs*/) {
  const char *stateText = SemanticPresentation::activityLabel(codex.activity);
  const char *activitySymbol =
      SemanticPresentation::activitySymbol(theme, codex.activity);
  char heading[28];
  if (activitySymbol && activitySymbol[0] != '\0' &&
      codex.activity != CodexActivityState::Unavailable) {
    snprintf(heading, sizeof(heading), "CODEX  %s %s", activitySymbol,
             stateText);
  } else {
    snprintf(heading, sizeof(heading), "CODEX  %s", stateText);
  }
  const uint16_t headingColor =
      SemanticPresentation::severityColor(theme, codex.severity);
  drawText(canvas, canvasReady, 4, 18, heading, headingColor);

  char elapsed[24];
  formatActivityElapsed(codex.elapsedSeconds, elapsed, sizeof(elapsed));
  if (elapsed[0]) {
    drawText(canvas, canvasReady, 4, 35, elapsed, theme.palette.primary);
  }

  char allowancePercent[8];
  const bool allowanceKnown =
      (codex.allowanceAvailability == DataAvailability::Available ||
       codex.allowanceAvailability == DataAvailability::Stale) &&
      codex.allowanceRemainingPercentage.known;
  if (allowanceKnown) {
    formatPercentage(codex.allowanceRemainingPercentage, allowancePercent,
                     sizeof(allowancePercent));
  } else {
    snprintf(allowancePercent, sizeof(allowancePercent), "N/A");
  }
  char metrics[24];
  snprintf(metrics, sizeof(metrics), "ALLOWANCE %s", allowancePercent);
  drawText(canvas, canvasReady, 4, 56, metrics,
           codex.allowanceAvailability == DataAvailability::Stale
               ? theme.palette.warning
               : theme.palette.primary);

  if (allowanceKnown && !codex.allowanceResetText.isEmpty()) {
    drawText(canvas, canvasReady, 4, 76, "RESET", theme.palette.muted);
    drawClippedText(canvas, canvasReady, 52, 76, 184,
                    codex.allowanceResetText, 0, theme.palette.secondary);
  } else if (allowanceKnown && codex.allowanceResetUnixSeconds.known) {
    char resetDate[8];
    char resetTime[8];
    if (formatResetDateTime(codex.allowanceResetUnixSeconds, resetDate,
                            sizeof(resetDate), resetTime,
                            sizeof(resetTime))) {
      char resetDateTime[16];
      snprintf(resetDateTime, sizeof(resetDateTime), "%s %s", resetDate,
               resetTime);
      drawText(canvas, canvasReady, 4, 76, "RESET", theme.palette.muted);
      drawText(canvas, canvasReady, 52, 76, resetDateTime,
               theme.palette.secondary);
    }
  } else if (codex.allowanceAvailability == DataAvailability::Loading) {
    drawText(canvas, canvasReady, 4, 76, "ALLOWANCE LOADING",
             theme.palette.info);
  } else if (codex.allowanceAvailability == DataAvailability::Stale) {
    drawText(canvas, canvasReady, 4, 76, "ALLOWANCE STALE",
             theme.palette.warning);
  }

  char updated[8];
  formatTime(codex.lastUpdateUnixSeconds, updated, sizeof(updated));
  drawText(canvas, canvasReady, 4, 96, "UPDATED", theme.palette.muted);
  drawText(canvas, canvasReady, 68, 96, updated, theme.palette.secondary);
}

void CompanionRenderer::drawMeetingScreen(M5Canvas &canvas, bool canvasReady,
                                          const CompanionUiModel &model,
                                          const ThemeStyle &theme,
                                          unsigned long nowMs) {
  const MeetingData &meeting = model.meeting;
  const bool backendOffline =
      model.header.connection != ConnectionState::Online;
  const char *stateText =
      meeting.availability == DataAvailability::Available
          ? SemanticPresentation::meetingStateLabel(meeting.state)
          : SemanticPresentation::availabilityLabel(meeting.availability);
  if (backendOffline && meeting.availability == DataAvailability::Stale) {
    stateText = "OFFLINE";
  }

  char heading[28];
  snprintf(heading, sizeof(heading), "MEETING  %s", stateText);
  drawText(canvas, canvasReady, 4, 18, heading,
           meeting.availability == DataAvailability::Available
               ? theme.palette.primary
               : SemanticPresentation::availabilityColor(
                     theme, meeting.availability));

  if (meeting.state == MeetingState::None &&
      meeting.availability == DataAvailability::Available) {
    drawText(canvas, canvasReady, 4, 48, "NO MEETING",
             theme.palette.success);
    drawText(canvas, canvasReady, 4, 68, "Calendar is clear",
             theme.palette.secondary);
    _meetingTitleMarquee.reset();
    _meetingAgendaMarquee.reset();
    return;
  }

  if ((meeting.availability == DataAvailability::Unknown ||
       meeting.availability == DataAvailability::Loading ||
       meeting.availability == DataAvailability::Unavailable ||
       meeting.availability == DataAvailability::Error) &&
      meeting.title.isEmpty()) {
    drawText(canvas, canvasReady, 4, 48,
             meeting.availability == DataAvailability::Loading
                 ? "Loading meeting data..."
                 : "Meeting data unavailable",
             SemanticPresentation::availabilityColor(theme,
                                                     meeting.availability));
    _meetingTitleMarquee.reset();
    _meetingAgendaMarquee.reset();
    return;
  }

  char startText[8];
  formatTime(meeting.startTimeUnixSeconds, startText, sizeof(startText));
  char timing[24] = "--";
  if (meeting.state == MeetingState::InProgress &&
      meeting.secondsRemaining.known) {
    formatDuration(meeting.secondsRemaining.value, "left", timing,
                   sizeof(timing));
  } else if (meeting.secondsUntilStart.known) {
    formatDuration(meeting.secondsUntilStart.value, "to go", timing,
                   sizeof(timing));
  }
  char schedule[32];
  snprintf(schedule, sizeof(schedule), "%s  %s", startText, timing);
  drawText(canvas, canvasReady, 4, 34, schedule, theme.palette.secondary);

  _meetingTitleMarquee.configure(meeting.title, kTextRightPx - kTextLeftPx,
                                  nowMs);
  drawClippedText(canvas, canvasReady, kTextLeftPx, 51,
                  kTextRightPx - kTextLeftPx, meeting.title,
                  _meetingTitleMarquee.offsetPixels(nowMs),
                  theme.palette.primary);
  _meetingTitleMarquee.noteRendered(nowMs);

  drawText(canvas, canvasReady, 4, 68,
           SemanticPresentation::locationTypeLabel(meeting.locationType),
           theme.palette.muted);
  if (meeting.location.isEmpty()) {
    drawText(canvas, canvasReady, 76, 68, "--", theme.palette.secondary);
  } else {
    drawClippedText(canvas, canvasReady, 76, 68, 160, meeting.location, 0,
                    theme.palette.secondary);
  }

  drawText(canvas, canvasReady, 4, 84, "AGENDA", theme.palette.muted);
  _meetingAgendaMarquee.configure(meeting.agendaSummary,
                                   kTextRightPx - kTextLeftPx, nowMs);
  drawClippedText(canvas, canvasReady, kTextLeftPx, 101,
                  kTextRightPx - kTextLeftPx, meeting.agendaSummary,
                  _meetingAgendaMarquee.offsetPixels(nowMs),
                  theme.palette.secondary);
  _meetingAgendaMarquee.noteRendered(nowMs);
}

void CompanionRenderer::drawFooter(M5Canvas &canvas, bool canvasReady,
                                   const ThemeStyle &theme) const {
  if (theme.borders.showSectionDividers) {
    drawHorizontalLine(canvas, canvasReady, 118, theme.palette.muted,
                       theme.borders.dividerThicknessPx);
  }
  drawText(canvas, canvasReady, theme.spacing.edgeInsetPx, 119, "A VIEW",
           theme.palette.muted);
  drawText(canvas, canvasReady, 124, 119, "HOLD B MENU",
           theme.palette.muted);
}

void CompanionRenderer::drawText(M5Canvas &canvas, bool canvasReady, int x,
                                 int y, const char *text,
                                 uint16_t color) const {
  if (canvasReady) {
    canvas.setTextColor(color);
    canvas.setCursor(x, y);
    canvas.print(text);
    return;
  }
  M5.Display.setTextColor(color);
  M5.Display.setCursor(x, y);
  M5.Display.print(text);
}

void CompanionRenderer::drawText(M5Canvas &canvas, bool canvasReady, int x,
                                 int y, const String &text,
                                 uint16_t color) const {
  drawText(canvas, canvasReady, x, y, text.c_str(), color);
}

void CompanionRenderer::drawScaledText(
    M5Canvas &canvas, bool canvasReady, int x, int y, const char *text,
    uint16_t color, uint8_t scale, const ThemeStyle &theme) const {
  const uint8_t safeScale = max<uint8_t>(1, scale);
  if (canvasReady) {
    canvas.setTextSize(safeScale);
    canvas.setTextColor(color);
    canvas.setCursor(x, y);
    canvas.print(text);
    canvas.setTextSize(theme.typography.textScale);
    return;
  }
  M5.Display.setTextSize(safeScale);
  M5.Display.setTextColor(color);
  M5.Display.setCursor(x, y);
  M5.Display.print(text);
  M5.Display.setTextSize(theme.typography.textScale);
}

void CompanionRenderer::drawClippedText(
    M5Canvas &canvas, bool canvasReady, int x, int y, int width,
    const String &text, int offsetPx, uint16_t color) const {
  if (canvasReady) {
    canvas.setClipRect(x, y, width, kLineHeightPx);
    canvas.setTextColor(color);
    canvas.setCursor(x - offsetPx, y);
    canvas.print(text);
    canvas.clearClipRect();
    return;
  }
  M5.Display.setClipRect(x, y, width, kLineHeightPx);
  M5.Display.setTextColor(color);
  M5.Display.setCursor(x - offsetPx, y);
  M5.Display.print(text);
    M5.Display.clearClipRect();
}

void CompanionRenderer::drawThemeMark(M5Canvas &canvas, bool canvasReady,
                                      int x, int y,
                                      const ThemeStyle &theme) const {
  if (theme.primitives.mark != ThemeMark::Wordmark) {
    return;
  }
  drawText(canvas, canvasReady, x, y + 1, "NERV", theme.palette.secondary);
}

void CompanionRenderer::drawAngledPanel(
    M5Canvas &canvas, bool canvasReady, int x, int y, int width, int height,
    uint16_t color, const ThemeStyle &theme) const {
  const int cut = constrain(static_cast<int>(theme.primitives.cornerCutPx), 0,
                            min(width, height) / 2);
  const int right = x + width - 1;
  const int bottom = y + height - 1;
  if (canvasReady) {
    canvas.drawLine(x, y, right - cut, y, color);
    canvas.drawLine(right - cut, y, right, y + cut, color);
    canvas.drawLine(right, y + cut, right, bottom, color);
    canvas.drawLine(right, bottom, x + cut, bottom, color);
    canvas.drawLine(x + cut, bottom, x, bottom - cut, color);
    canvas.drawLine(x, bottom - cut, x, y, color);
  } else {
    M5.Display.drawLine(x, y, right - cut, y, color);
    M5.Display.drawLine(right - cut, y, right, y + cut, color);
    M5.Display.drawLine(right, y + cut, right, bottom, color);
    M5.Display.drawLine(right, bottom, x + cut, bottom, color);
    M5.Display.drawLine(x + cut, bottom, x, bottom - cut, color);
    M5.Display.drawLine(x, bottom - cut, x, y, color);
  }
}

void CompanionRenderer::drawSideRail(M5Canvas &canvas, bool canvasReady,
                                     int height,
                                     const ThemeStyle &theme) const {
  const int railWidth = theme.primitives.sideRailWidthPx;
  if (railWidth <= 0) {
    return;
  }
  int markerIndex = 0;
  for (int y = 0; y < height; y += 18, ++markerIndex) {
    const int markerHeight = min(12, height - y);
    const uint16_t markerColor =
        markerIndex % 4 == 3
            ? theme.palette.warning
            : markerIndex % 2 == 0 ? theme.palette.secondary
                                   : theme.palette.info;
    if (canvasReady) {
      canvas.fillRect(0, y, railWidth, markerHeight, markerColor);
    } else {
      M5.Display.fillRect(0, y, railWidth, markerHeight, markerColor);
    }
  }
}

void CompanionRenderer::drawSegmentedProgress(
    M5Canvas &canvas, bool canvasReady, int x, int y, int width,
    const PercentageValue &value, uint16_t color,
    const ThemeStyle &theme) const {
  const int segments = max(1, static_cast<int>(theme.progress.segmentCount));
  const int gap = segments > 1 ? 2 : 0;
  const int segmentWidth = max(1, (width - gap * (segments - 1)) / segments);
  const int filled = value.known
                         ? constrain((static_cast<int>(value.value) * segments +
                                      99) /
                                         100,
                                     0, segments)
                         : 0;
  for (int i = 0; i < segments; ++i) {
    const int segmentX = x + i * (segmentWidth + gap);
    if (canvasReady) {
      if (!theme.progress.outlined) {
        if (i < filled) {
          canvas.fillRect(segmentX, y, segmentWidth,
                          theme.progress.barHeightPx, color);
        } else {
          canvas.drawFastHLine(segmentX,
                               y + theme.progress.barHeightPx / 2,
                               segmentWidth, theme.palette.muted);
        }
      } else {
        canvas.drawRect(segmentX, y, segmentWidth,
                        theme.progress.barHeightPx, theme.palette.muted);
        if (i < filled && segmentWidth > 2 &&
            theme.progress.barHeightPx > 2) {
          canvas.fillRect(segmentX + 1, y + 1, segmentWidth - 2,
                          theme.progress.barHeightPx - 2, color);
        }
      }
    } else {
      if (!theme.progress.outlined) {
        if (i < filled) {
          M5.Display.fillRect(segmentX, y, segmentWidth,
                              theme.progress.barHeightPx, color);
        } else {
          M5.Display.drawFastHLine(segmentX,
                                   y + theme.progress.barHeightPx / 2,
                                   segmentWidth, theme.palette.muted);
        }
      } else {
        M5.Display.drawRect(segmentX, y, segmentWidth,
                            theme.progress.barHeightPx,
                            theme.palette.muted);
        if (i < filled && segmentWidth > 2 &&
            theme.progress.barHeightPx > 2) {
          M5.Display.fillRect(segmentX + 1, y + 1, segmentWidth - 2,
                              theme.progress.barHeightPx - 2, color);
        }
      }
    }
  }
}

void CompanionRenderer::drawBatteryIcon(M5Canvas &canvas, bool canvasReady,
                                        int x, int y,
                                        uint16_t color) const {
  if (canvasReady) {
    canvas.drawRect(x, y, 8, 8, color);
    canvas.fillRect(x + 8, y + 2, 2, 4, color);
  } else {
    M5.Display.drawRect(x, y, 8, 8, color);
    M5.Display.fillRect(x + 8, y + 2, 2, 4, color);
  }
}

void CompanionRenderer::drawHorizontalLine(M5Canvas &canvas, bool canvasReady,
                                           int y, uint16_t color,
                                           int thickness) const {
  const int safeThickness = max(1, thickness);
  for (int row = 0; row < safeThickness; ++row) {
    if (canvasReady) {
      canvas.drawFastHLine(0, y + row, displayWidth(canvas, true), color);
    } else {
      M5.Display.drawFastHLine(0, y + row, displayWidth(canvas, false), color);
    }
  }
}

int CompanionRenderer::displayWidth(M5Canvas &canvas,
                                    bool canvasReady) const {
  return canvasReady ? canvas.width() : M5.Display.width();
}

int CompanionRenderer::displayHeight(M5Canvas &canvas,
                                     bool canvasReady) const {
  return canvasReady ? canvas.height() : M5.Display.height();
}

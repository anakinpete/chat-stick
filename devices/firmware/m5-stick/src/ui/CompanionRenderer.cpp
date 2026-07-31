#include "CompanionRenderer.h"

#include "../Config.h"
#include "state/SemanticPresentation.h"
#include <stdio.h>
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

void formatPercentage(const PercentageValue &value, char *buffer,
                      size_t bufferSize) {
  if (!value.known) {
    snprintf(buffer, bufferSize, "--");
    return;
  }
  snprintf(buffer, bufferSize, "%u%%", static_cast<unsigned>(value.value));
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

} // namespace

void CompanionRenderer::render(M5Canvas &canvas, bool canvasReady,
                               const CompanionUiModel &model,
                               const ThemeStyle &theme,
                               unsigned long nowMs) {
  if (!_hasRenderedScreen || _lastScreen != model.activeScreen) {
    _codexTaskMarquee.reset();
    _meetingTitleMarquee.reset();
    _meetingAgendaMarquee.reset();
    _lastScreen = model.activeScreen;
    _hasRenderedScreen = true;
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
    return _codexTaskMarquee.needsFrame(nowMs);
  }
  return _meetingTitleMarquee.needsFrame(nowMs) ||
         _meetingAgendaMarquee.needsFrame(nowMs);
}

void CompanionRenderer::reset() {
  _hasRenderedScreen = false;
  _codexTaskMarquee.reset();
  _meetingTitleMarquee.reset();
  _meetingAgendaMarquee.reset();
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
                                        unsigned long nowMs) {
  const char *stateText = codex.availability == DataAvailability::Available
                              ? SemanticPresentation::activityLabel(
                                    codex.activity)
                              : SemanticPresentation::availabilityLabel(
                                    codex.availability);
  const char *activitySymbol =
      SemanticPresentation::activitySymbol(theme, codex.activity);
  char heading[28];
  if (activitySymbol && activitySymbol[0] != '\0' &&
      codex.availability == DataAvailability::Available) {
    snprintf(heading, sizeof(heading), "CODEX  %s %s", activitySymbol,
             stateText);
  } else {
    snprintf(heading, sizeof(heading), "CODEX  %s", stateText);
  }
  const uint16_t headingColor =
      codex.availability == DataAvailability::Available
          ? SemanticPresentation::severityColor(theme, codex.severity)
          : SemanticPresentation::availabilityColor(theme,
                                                     codex.availability);
  drawText(canvas, canvasReady, 4, 18, heading, headingColor);

  drawText(canvas, canvasReady, 4, 35, "TASK", theme.palette.muted);
  if (codex.currentTaskTitle.isEmpty()) {
    _codexTaskMarquee.reset();
    drawText(canvas, canvasReady, 44, 35, "No active task",
             theme.palette.primary);
  } else {
    _codexTaskMarquee.configure(codex.currentTaskTitle, 192, nowMs);
    drawClippedText(canvas, canvasReady, 44, 35, 192,
                    codex.currentTaskTitle,
                    _codexTaskMarquee.offsetPixels(nowMs),
                    theme.palette.primary);
    _codexTaskMarquee.noteRendered(nowMs);
  }

  char taskPercent[8];
  char allowancePercent[8];
  formatPercentage(codex.taskProgressPercentage, taskPercent,
                   sizeof(taskPercent));
  if (codex.allowanceAvailability == DataAvailability::Available &&
      codex.allowanceRemainingPercentage.known) {
    formatPercentage(codex.allowanceRemainingPercentage, allowancePercent,
                     sizeof(allowancePercent));
  } else {
    snprintf(allowancePercent, sizeof(allowancePercent), "N/A");
  }
  char metrics[32];
  snprintf(metrics, sizeof(metrics), "TASK %s  ALLOWANCE %s", taskPercent,
           allowancePercent);
  drawText(canvas, canvasReady, 4, 56, metrics, theme.palette.primary);

  char resetTime[8];
  if (codex.allowanceAvailability == DataAvailability::Available &&
      !codex.allowanceResetText.isEmpty()) {
    drawText(canvas, canvasReady, 4, 76, "RESET", theme.palette.muted);
    drawClippedText(canvas, canvasReady, 52, 76, 184,
                    codex.allowanceResetText, 0, theme.palette.secondary);
  } else if (codex.allowanceAvailability == DataAvailability::Available &&
             codex.allowanceResetUnixSeconds.known) {
    drawText(canvas, canvasReady, 4, 76, "RESET", theme.palette.muted);
    formatTime(codex.allowanceResetUnixSeconds, resetTime, sizeof(resetTime));
    drawText(canvas, canvasReady, 52, 76, resetTime,
             theme.palette.secondary);
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

void CompanionRenderer::drawHorizontalLine(M5Canvas &canvas, bool canvasReady,
                                           int y, uint16_t color,
                                           int thickness) const {
  const int safeThickness = max(1, thickness);
  for (int row = 0; row < safeThickness; ++row) {
    if (canvasReady) {
      canvas.drawFastHLine(0, y + row, SCREEN_WIDTH_PX, color);
    } else {
      M5.Display.drawFastHLine(0, y + row, SCREEN_WIDTH_PX, color);
    }
  }
}

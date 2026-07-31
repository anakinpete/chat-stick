#include "CompanionRenderer.h"

#include "../Config.h"
#include <stdio.h>
#include <time.h>

namespace {
constexpr uint16_t COLOR_PRIMARY = 0xFFFF;
constexpr uint16_t COLOR_SECONDARY = 0xBDF7;
constexpr uint16_t COLOR_MUTED = 0x7BEF;
constexpr uint16_t COLOR_INFO = 0x07FF;
constexpr uint16_t COLOR_SUCCESS = 0x07E0;
constexpr uint16_t COLOR_WARNING = 0xFFE0;
constexpr uint16_t COLOR_ERROR = 0xF800;

const char *connectionLabel(ConnectionState state) {
  switch (state) {
  case ConnectionState::Offline:
    return "OFF";
  case ConnectionState::WiFiOnly:
    return "WIFI";
  case ConnectionState::BackendConnecting:
    return "LINK";
  case ConnectionState::Online:
    return "NET";
  }
  return "OFF";
}

uint16_t connectionColor(ConnectionState state) {
  switch (state) {
  case ConnectionState::Online:
    return COLOR_SUCCESS;
  case ConnectionState::BackendConnecting:
  case ConnectionState::WiFiOnly:
    return COLOR_WARNING;
  case ConnectionState::Offline:
  default:
    return COLOR_ERROR;
  }
}

const char *weatherLabel(WeatherCondition condition) {
  switch (condition) {
  case WeatherCondition::Clear:
    return "SUN";
  case WeatherCondition::PartlyCloudy:
    return "PCL";
  case WeatherCondition::Cloudy:
    return "CLD";
  case WeatherCondition::Rain:
    return "RAIN";
  case WeatherCondition::Storm:
    return "STM";
  case WeatherCondition::Snow:
    return "SNOW";
  case WeatherCondition::Fog:
    return "FOG";
  case WeatherCondition::Wind:
    return "WND";
  case WeatherCondition::Unknown:
  default:
    return "WX";
  }
}

const char *activityLabel(CodexActivityState state) {
  switch (state) {
  case CodexActivityState::Idle:
    return "IDLE";
  case CodexActivityState::Working:
    return "WORKING";
  case CodexActivityState::Waiting:
    return "WAITING";
  case CodexActivityState::Complete:
    return "COMPLETE";
  case CodexActivityState::Error:
    return "ERROR";
  case CodexActivityState::Unavailable:
  default:
    return "UNAVAILABLE";
  }
}

const char *availabilityLabel(DataAvailability state) {
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

uint16_t availabilityColor(DataAvailability state) {
  switch (state) {
  case DataAvailability::Loading:
    return COLOR_INFO;
  case DataAvailability::Available:
    return COLOR_SUCCESS;
  case DataAvailability::Stale:
    return COLOR_WARNING;
  case DataAvailability::Error:
    return COLOR_ERROR;
  case DataAvailability::Unavailable:
  case DataAvailability::Unknown:
  default:
    return COLOR_MUTED;
  }
}

uint16_t severityColor(UiSeverity severity) {
  switch (severity) {
  case UiSeverity::Info:
    return COLOR_INFO;
  case UiSeverity::Warning:
    return COLOR_WARNING;
  case UiSeverity::Success:
    return COLOR_SUCCESS;
  case UiSeverity::Error:
    return COLOR_ERROR;
  case UiSeverity::Normal:
  default:
    return COLOR_PRIMARY;
  }
}

const char *severityLabel(UiSeverity severity) {
  switch (severity) {
  case UiSeverity::Info:
    return "INFO ";
  case UiSeverity::Warning:
    return "WARN ";
  case UiSeverity::Success:
    return "OK ";
  case UiSeverity::Error:
    return "ERR ";
  case UiSeverity::Normal:
  default:
    return "";
  }
}

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

const char *meetingStateLabel(MeetingState state) {
  switch (state) {
  case MeetingState::None:
    return "NONE";
  case MeetingState::Upcoming:
    return "NEXT";
  case MeetingState::InProgress:
    return "NOW";
  case MeetingState::Finished:
    return "FINISHED";
  case MeetingState::Unavailable:
  default:
    return "UNAVAILABLE";
  }
}

const char *locationTypeLabel(MeetingLocationType type) {
  switch (type) {
  case MeetingLocationType::Physical:
    return "PLACE";
  case MeetingLocationType::VideoCall:
    return "VIDEO";
  case MeetingLocationType::PhoneCall:
    return "PHONE";
  case MeetingLocationType::Hybrid:
    return "HYBRID";
  case MeetingLocationType::Other:
    return "OTHER";
  case MeetingLocationType::Unknown:
  default:
    return "WHERE";
  }
}
} // namespace

void CompanionRenderer::render(M5Canvas &canvas, bool canvasReady,
                               const CompanionUiModel &model,
                               unsigned long nowMs) {
  if (!_hasRenderedScreen || _lastScreen != model.activeScreen) {
    _meetingTitleMarquee.reset();
    _meetingAgendaMarquee.reset();
    _lastScreen = model.activeScreen;
    _hasRenderedScreen = true;
  }

  drawHeader(canvas, canvasReady, model.header);
  if (model.activeScreen == PrimaryScreen::Codex) {
    drawCodexScreen(canvas, canvasReady, model.codex);
  } else {
    drawMeetingScreen(canvas, canvasReady, model, nowMs);
  }
  drawFooter(canvas, canvasReady);
}

bool CompanionRenderer::needsFrame(unsigned long nowMs) const {
  if (!_hasRenderedScreen || _lastScreen != PrimaryScreen::Meeting) {
    return false;
  }
  return _meetingTitleMarquee.needsFrame(nowMs) ||
         _meetingAgendaMarquee.needsFrame(nowMs);
}

void CompanionRenderer::reset() {
  _hasRenderedScreen = false;
  _meetingTitleMarquee.reset();
  _meetingAgendaMarquee.reset();
}

void CompanionRenderer::drawHeader(M5Canvas &canvas, bool canvasReady,
                                   const HeaderData &header) const {
  char timeText[8];
  formatTime(header.currentTimeUnixSeconds, timeText, sizeof(timeText));
  drawText(canvas, canvasReady, 4, 0, timeText, COLOR_PRIMARY);

  char weatherText[14];
  if (header.weather.availability == DataAvailability::Available ||
      header.weather.availability == DataAvailability::Stale) {
    if (header.weather.temperature.known) {
      const char unit = header.weather.temperatureUnit ==
                                TemperatureUnit::Fahrenheit
                            ? 'F'
                            : 'C';
      snprintf(weatherText, sizeof(weatherText), "%s %.0f%c",
               weatherLabel(header.weather.condition),
               header.weather.temperature.value, unit);
    } else {
      snprintf(weatherText, sizeof(weatherText), "%s --",
               weatherLabel(header.weather.condition));
    }
  } else {
    snprintf(weatherText, sizeof(weatherText), "WX --");
  }
  drawText(canvas, canvasReady, 52, 0, weatherText, COLOR_SECONDARY);

  char batteryText[8];
  if (header.batteryPercentage.known) {
    snprintf(batteryText, sizeof(batteryText), "B%u",
             static_cast<unsigned>(header.batteryPercentage.value));
  } else {
    snprintf(batteryText, sizeof(batteryText), "B--");
  }
  drawText(canvas, canvasReady, 148, 0, batteryText, COLOR_SECONDARY);
  drawText(canvas, canvasReady, 204, 0, connectionLabel(header.connection),
           connectionColor(header.connection));
  drawHorizontalLine(canvas, canvasReady, 16, COLOR_MUTED);
}

void CompanionRenderer::drawCodexScreen(M5Canvas &canvas, bool canvasReady,
                                        const CodexStatusData &codex) const {
  const char *stateText = codex.availability == DataAvailability::Available
                              ? activityLabel(codex.activity)
                              : availabilityLabel(codex.availability);
  char heading[28];
  snprintf(heading, sizeof(heading), "CODEX  %s%s",
           codex.availability == DataAvailability::Available
               ? severityLabel(codex.severity)
               : "",
           stateText);
  const uint16_t headingColor =
      codex.availability == DataAvailability::Available
          ? severityColor(codex.severity)
          : availabilityColor(codex.availability);
  drawText(canvas, canvasReady, 4, 18, heading, headingColor);

  drawText(canvas, canvasReady, 4, 35, "TASK", COLOR_MUTED);
  if (codex.currentTaskTitle.isEmpty()) {
    drawText(canvas, canvasReady, 44, 35, "No active task", COLOR_PRIMARY);
  } else {
    drawClippedText(canvas, canvasReady, 44, 35, 192,
                    codex.currentTaskTitle, 0, COLOR_PRIMARY);
  }

  char taskPercent[8];
  char allowancePercent[8];
  formatPercentage(codex.taskProgressPercentage, taskPercent,
                   sizeof(taskPercent));
  formatPercentage(codex.allowanceRemainingPercentage, allowancePercent,
                   sizeof(allowancePercent));
  char metrics[32];
  snprintf(metrics, sizeof(metrics), "PROG %s   ALLOW %s", taskPercent,
           allowancePercent);
  drawText(canvas, canvasReady, 4, 56, metrics, COLOR_PRIMARY);

  char resetTime[8];
  drawText(canvas, canvasReady, 4, 76, "RESET", COLOR_MUTED);
  if (!codex.allowanceResetText.isEmpty()) {
    drawClippedText(canvas, canvasReady, 52, 76, 184,
                    codex.allowanceResetText, 0, COLOR_SECONDARY);
  } else if (codex.allowanceResetUnixSeconds.known) {
    formatTime(codex.allowanceResetUnixSeconds, resetTime, sizeof(resetTime));
    drawText(canvas, canvasReady, 52, 76, resetTime, COLOR_SECONDARY);
  } else {
    drawText(canvas, canvasReady, 52, 76, "--", COLOR_SECONDARY);
  }

  char updated[8];
  formatTime(codex.lastUpdateUnixSeconds, updated, sizeof(updated));
  drawText(canvas, canvasReady, 4, 96, "UPDATED", COLOR_MUTED);
  drawText(canvas, canvasReady, 68, 96, updated, COLOR_SECONDARY);
}

void CompanionRenderer::drawMeetingScreen(M5Canvas &canvas, bool canvasReady,
                                          const CompanionUiModel &model,
                                          unsigned long nowMs) {
  const MeetingData &meeting = model.meeting;
  const bool backendOffline =
      model.header.connection != ConnectionState::Online;
  const char *stateText =
      meeting.availability == DataAvailability::Available
          ? meetingStateLabel(meeting.state)
          : availabilityLabel(meeting.availability);
  if (backendOffline && meeting.availability == DataAvailability::Stale) {
    stateText = "OFFLINE";
  }

  char heading[28];
  snprintf(heading, sizeof(heading), "MEETING  %s", stateText);
  drawText(canvas, canvasReady, 4, 18, heading,
           meeting.availability == DataAvailability::Available
               ? COLOR_PRIMARY
               : availabilityColor(meeting.availability));

  if (meeting.state == MeetingState::None &&
      meeting.availability == DataAvailability::Available) {
    drawText(canvas, canvasReady, 4, 48, "NO MEETING", COLOR_SUCCESS);
    drawText(canvas, canvasReady, 4, 68, "Calendar is clear",
             COLOR_SECONDARY);
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
             availabilityColor(meeting.availability));
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
  drawText(canvas, canvasReady, 4, 34, schedule, COLOR_SECONDARY);

  _meetingTitleMarquee.configure(meeting.title, kTextRightPx - kTextLeftPx,
                                  nowMs);
  drawClippedText(canvas, canvasReady, kTextLeftPx, 51,
                  kTextRightPx - kTextLeftPx, meeting.title,
                  _meetingTitleMarquee.offsetPixels(nowMs), COLOR_PRIMARY);
  _meetingTitleMarquee.noteRendered(nowMs);

  drawText(canvas, canvasReady, 4, 68,
           locationTypeLabel(meeting.locationType), COLOR_MUTED);
  if (meeting.location.isEmpty()) {
    drawText(canvas, canvasReady, 52, 68, "--", COLOR_SECONDARY);
  } else {
    drawClippedText(canvas, canvasReady, 52, 68, 184, meeting.location, 0,
                    COLOR_SECONDARY);
  }

  drawText(canvas, canvasReady, 4, 84, "AGENDA", COLOR_MUTED);
  _meetingAgendaMarquee.configure(meeting.agendaSummary,
                                   kTextRightPx - kTextLeftPx, nowMs);
  drawClippedText(canvas, canvasReady, kTextLeftPx, 101,
                  kTextRightPx - kTextLeftPx, meeting.agendaSummary,
                  _meetingAgendaMarquee.offsetPixels(nowMs), COLOR_SECONDARY);
  _meetingAgendaMarquee.noteRendered(nowMs);
}

void CompanionRenderer::drawFooter(M5Canvas &canvas, bool canvasReady) const {
  drawHorizontalLine(canvas, canvasReady, 118, COLOR_MUTED);
  drawText(canvas, canvasReady, 4, 119, "A VIEW", COLOR_MUTED);
  drawText(canvas, canvasReady, 124, 119, "HOLD B MENU", COLOR_MUTED);
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
                                           int y, uint16_t color) const {
  if (canvasReady) {
    canvas.drawFastHLine(0, y, SCREEN_WIDTH_PX, color);
    return;
  }
  M5.Display.drawFastHLine(0, y, SCREEN_WIDTH_PX, color);
}

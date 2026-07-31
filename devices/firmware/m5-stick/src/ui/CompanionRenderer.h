#pragma once

#include "TextMarquee.h"
#include "state/CompanionUiModel.h"
#include "state/Theme.h"
#include <M5Unified.h>

/**
 * @brief Plain theme-neutral renderer for the two companion screens.
 */
class CompanionRenderer {
public:
  void render(M5Canvas &canvas, bool canvasReady,
              const CompanionUiModel &model, const ThemeStyle &theme,
              unsigned long nowMs);
  bool needsFrame(unsigned long nowMs) const;
  void reset();

private:
  static constexpr int kLineHeightPx = 16;
  static constexpr int kTextLeftPx = 4;
  static constexpr int kTextRightPx = 236;

  PrimaryScreen _lastScreen = PrimaryScreen::Codex;
  bool _hasRenderedScreen = false;
  TextMarquee _codexTaskMarquee;
  TextMarquee _meetingTitleMarquee;
  TextMarquee _meetingAgendaMarquee;

  void drawHeader(M5Canvas &canvas, bool canvasReady,
                  const HeaderData &header, const ThemeStyle &theme) const;
  void drawCodexScreen(M5Canvas &canvas, bool canvasReady,
                       const CodexStatusData &codex, const ThemeStyle &theme,
                       unsigned long nowMs);
  void drawMeetingScreen(M5Canvas &canvas, bool canvasReady,
                         const CompanionUiModel &model,
                         const ThemeStyle &theme,
                         unsigned long nowMs);
  void drawFooter(M5Canvas &canvas, bool canvasReady,
                  const ThemeStyle &theme) const;

  void drawText(M5Canvas &canvas, bool canvasReady, int x, int y,
                const char *text, uint16_t color) const;
  void drawText(M5Canvas &canvas, bool canvasReady, int x, int y,
                const String &text, uint16_t color) const;
  void drawClippedText(M5Canvas &canvas, bool canvasReady, int x, int y,
                       int width, const String &text, int offsetPx,
                       uint16_t color) const;
  void drawHorizontalLine(M5Canvas &canvas, bool canvasReady, int y,
                          uint16_t color, int thickness) const;
};

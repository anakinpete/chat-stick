#pragma once

#include <Arduino.h>

/**
 * @brief Elapsed-time-driven horizontal text marquee state.
 *
 * Drawing is owned by the renderer. This helper only tracks whether scrolling
 * is needed and the current pixel offset.
 */
class TextMarquee {
public:
  void configure(const String &text, int viewportWidthPx,
                 unsigned long nowMs);
  void reset();

  int offsetPixels(unsigned long nowMs);
  bool needsFrame(unsigned long nowMs) const;
  void noteRendered(unsigned long nowMs);
  bool active() const { return _active; }

private:
  enum class Phase { StartPause, Scrolling, EndPause };

  static constexpr int kGlyphWidthPx = 8;
  static constexpr int kScrollPixelsPerSecond = 28;
  static constexpr unsigned long kStartPauseMs = 900;
  static constexpr unsigned long kEndPauseMs = 800;
  static constexpr unsigned long kFrameIntervalMs = 70;

  String _text;
  int _viewportWidthPx = 0;
  int _maximumOffsetPx = 0;
  int _offsetPx = 0;
  bool _active = false;
  Phase _phase = Phase::StartPause;
  unsigned long _phaseStartMs = 0;
  unsigned long _lastRenderMs = 0;
};

#include "TextMarquee.h"

void TextMarquee::configure(const String &text, int viewportWidthPx,
                            unsigned long nowMs) {
  if (_text == text && _viewportWidthPx == viewportWidthPx) {
    return;
  }

  _text = text;
  _viewportWidthPx = max(0, viewportWidthPx);
  _maximumOffsetPx =
      max(0, static_cast<int>(_text.length()) * kGlyphWidthPx -
                 _viewportWidthPx);
  _active = _maximumOffsetPx > 0;
  _offsetPx = 0;
  _phase = Phase::StartPause;
  _phaseStartMs = nowMs;
  _lastRenderMs = 0;
}

void TextMarquee::reset() {
  _text = "";
  _viewportWidthPx = 0;
  _maximumOffsetPx = 0;
  _offsetPx = 0;
  _active = false;
  _phase = Phase::StartPause;
  _phaseStartMs = 0;
  _lastRenderMs = 0;
}

int TextMarquee::offsetPixels(unsigned long nowMs) {
  if (!_active) {
    return 0;
  }

  const unsigned long phaseElapsed = nowMs - _phaseStartMs;
  switch (_phase) {
  case Phase::StartPause:
    if (phaseElapsed >= kStartPauseMs) {
      _phase = Phase::Scrolling;
      _phaseStartMs = nowMs;
    }
    _offsetPx = 0;
    break;

  case Phase::Scrolling: {
    const unsigned long travelled =
        phaseElapsed * kScrollPixelsPerSecond / 1000;
    if (travelled >= static_cast<unsigned long>(_maximumOffsetPx)) {
      _offsetPx = _maximumOffsetPx;
      _phase = Phase::EndPause;
      _phaseStartMs = nowMs;
    } else {
      _offsetPx = static_cast<int>(travelled);
    }
    break;
  }

  case Phase::EndPause:
    if (phaseElapsed >= kEndPauseMs) {
      _phase = Phase::StartPause;
      _phaseStartMs = nowMs;
      _offsetPx = 0;
    }
    break;
  }

  return _offsetPx;
}

bool TextMarquee::needsFrame(unsigned long nowMs) const {
  if (!_active) {
    return false;
  }

  const unsigned long phaseElapsed = nowMs - _phaseStartMs;
  if (_phase == Phase::StartPause) {
    return phaseElapsed >= kStartPauseMs;
  }
  if (_phase == Phase::EndPause) {
    return phaseElapsed >= kEndPauseMs;
  }
  return _lastRenderMs == 0 || nowMs - _lastRenderMs >= kFrameIntervalMs;
}

void TextMarquee::noteRendered(unsigned long nowMs) {
  _lastRenderMs = nowMs;
}

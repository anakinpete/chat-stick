#pragma once

#include "../Config.h"
#include "CompanionRenderer.h"
#include "state/StateTypes.h"
#include <Arduino.h>
#include <M5Unified.h>

class TextDisplay {
public:
  static constexpr int kCharsPerLine = 29;
  static constexpr int kLines = 8;
  static constexpr int kChatRows = 7;
  // Body image bounding box, matching designs.md "Images" / Layout sections.
  static constexpr int kImageX = 4;
  static constexpr int kImageY = 4;
  static constexpr int kImageW = 232;
  static constexpr int kImageH = 112;

  // Custom 8x16 glyphs grafted onto the ASCII font. These codepoints are
  // unused control characters in normal text, so we repurpose them as
  // printable iconography and pass them through fitLine / wrapBodyText.
  static constexpr char kGlyphTriangleDown = '\x01';
  static constexpr char kGlyphBulletFilled = '\x02';
  static constexpr char kGlyphBulletHollow = '\x03';

  void init();
  void setBrightness(uint8_t brightness);
  void render(const DisplayState &state, const ThemeStyle &theme);
  void renderCompanion(const CompanionUiModel &model, const ThemeStyle &theme,
                       unsigned long nowMs);
  bool companionNeedsFrame(unsigned long nowMs) const;
  int pageCountForText(const String &text) const;
  String layoutTextForReveal(const String &text) const;
  int wrappedRowCount(const String &text) const;

  // Store a 1-bit packed bitmap (MSB first) for the body image area. Pixel
  // dimensions must match kImageW x kImageH; mismatches return false.
  bool setImage(const uint8_t *packed, size_t packedLen, int width, int height);
  void clearImage();
  bool hasImage() const { return _imageBuffer != nullptr; }

private:
  static constexpr int kBodyRows = 7;
  static constexpr int kFooterRow = 7;
  static constexpr size_t kCanvasBytes =
      static_cast<size_t>(SCREEN_WIDTH_PX) * SCREEN_HEIGHT_PX * sizeof(uint16_t);

  mutable M5Canvas _canvas;
  bool _canvasReady = false;
  ThemeOrientation _orientation = ThemeOrientation::Landscape;
  int _canvasWidth = SCREEN_WIDTH_PX;
  int _canvasHeight = SCREEN_HEIGHT_PX;
  uint16_t *_previousCanvas = nullptr;
  bool _hasPreviousCanvas = false;

  uint8_t *_imageBuffer = nullptr;
  size_t _imageBufferSize = 0;
  int _imageWidth = 0;
  int _imageHeight = 0;
  CompanionRenderer _companionRenderer;

  String fitLine(const String &text) const;
  String mergeEdgeText(const String &left, const String &right) const;
  String spaces(int count) const;
  int wrapBodyText(const String &text, String out[], int maxRows) const;
  void ensureOrientation(ThemeOrientation orientation);
  void flushCanvas(bool forceFull = false);
  void drawLine(int row, const String &text, uint16_t color) const;
  void drawCharCell(int x, int yTop, char c, uint16_t color) const;
  void drawBitmapGlyph(int x, int yTop, const uint8_t *bits,
                       uint16_t color) const;
  void drawGlyphAtRight(int row, char glyph, uint16_t color) const;
  void drawPageIndicator(int pageIndex, int pageCount) const;
  void drawMenu(const DisplayState &state, const ThemeStyle &theme) const;
  void drawStoredImage() const;
  void drawAlarm(const DisplayState &state) const;
  void drawBellIcon(int cx, int cy, uint16_t color) const;
};

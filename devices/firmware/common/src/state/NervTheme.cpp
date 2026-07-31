#include "NervTheme.h"

const ThemeStyle &nervThemeStyle() {
  static const ThemeStyle style = [] {
    ThemeStyle value;
    value.id = ThemeId::Nerv;
    value.identifier = "nerv";
    value.displayName = "NERV";

    value.palette.background = 0x0000; // Black.
    value.palette.primary = 0xFFDF;    // Pale essential text.
    value.palette.secondary = 0xF982;  // Vivid red-orange structure.
    value.palette.muted = 0x8C92;      // Neutral technical grey.
    value.palette.info = 0x06DB;       // Cyan technical accent.
    value.palette.success = 0x06DB;    // Cyan online/success state.
    value.palette.warning = 0xFFE0;    // Semantic warning yellow.
    value.palette.error = 0xF800;      // Semantic critical red.

    value.typography.bodyFont = ThemeFontRole::CompactReadable;
    value.typography.textScale = 1;
    value.typography.lineHeightPx = 16;
    value.spacing.edgeInsetPx = 5;
    value.spacing.smallGapPx = 3;
    value.spacing.mediumGapPx = 6;
    value.borders.showSectionDividers = true;
    value.borders.dividerThicknessPx = 2;
    value.borders.panelShape = ThemePanelShape::Angled;
    value.progress.barHeightPx = 8;
    value.progress.outlined = true;
    value.progress.segmentCount = 10;
    value.alerts.treatment = ThemeAlertTreatment::Outline;
    value.symbols.indicatorMode = ThemeIndicatorMode::Text;
    value.symbols.battery = "BAT";

    value.primitives.orientation = ThemeOrientation::Portrait;
    value.primitives.composition = ThemeComposition::Stacked;
    value.primitives.mark = ThemeMark::Wordmark;
    value.primitives.cornerCutPx = 7;
    value.primitives.sideRailWidthPx = 3;
    value.primitives.primaryTextScale = 2;
    return value;
  }();
  return style;
}

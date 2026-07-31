#include "BaseTheme.h"

const ThemeStyle &baseThemeStyle() {
  static const ThemeStyle style = [] {
    ThemeStyle value;
    value.id = ThemeId::Plain;
    value.identifier = "plain";
    value.displayName = "Plain";
    value.palette.background = 0x0000;
    value.palette.primary = 0xFFFF;
    value.palette.secondary = 0xBDF7;
    value.palette.muted = 0x7BEF;
    value.palette.info = 0x07FF;
    value.palette.success = 0x07E0;
    value.palette.warning = 0xFFE0;
    value.palette.error = 0xF800;
    value.typography.bodyFont = ThemeFontRole::CompactReadable;
    value.typography.textScale = 1;
    value.typography.lineHeightPx = 16;
    value.spacing.edgeInsetPx = 4;
    value.spacing.smallGapPx = 4;
    value.spacing.mediumGapPx = 8;
    value.borders.showSectionDividers = true;
    value.borders.dividerThicknessPx = 1;
    value.borders.panelShape = ThemePanelShape::Flat;
    value.progress.barHeightPx = 4;
    value.progress.outlined = true;
    value.alerts.treatment = ThemeAlertTreatment::TextOnly;
    value.symbols.indicatorMode = ThemeIndicatorMode::Text;
    return value;
  }();
  return style;
}

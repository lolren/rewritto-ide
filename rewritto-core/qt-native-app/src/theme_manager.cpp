#include "theme_manager.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QColor>
#include <QGuiApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QToolTip>

namespace {
bool g_inited = false;
QPalette g_defaultPalette;
QString g_defaultStyleKey;

struct ThemeSpec final {
  bool dark = false;
  QString windowBg;
  QString text;
  QString surface;
  QString surfaceAlt;
  QString accent;
  QString border;
  QString headerBg;
  QString headerFg;
  QString hover;
  QString listSelection;
  QString separator;
  QString listSelectionText;
  QString accentText;
  QString disabledText;
  QString alternateBase;
};

QString normalizeStyleKey(const QString& key) {
  if (key.trimmed().isEmpty()) {
    return {};
  }
  const QString wanted = key.trimmed();
  const QString wantedLower = wanted.toLower();
  for (const QString& k : QStyleFactory::keys()) {
    if (k.toLower() == wantedLower) {
      return k;
    }
  }
  return {};
}

double linearizeChannel(double value) {
  if (value <= 0.04045) {
    return value / 12.92;
  }
  return std::pow((value + 0.055) / 1.055, 2.4);
}

double relativeLuminance(const QColor& color) {
  const double r = linearizeChannel(color.redF());
  const double g = linearizeChannel(color.greenF());
  const double b = linearizeChannel(color.blueF());
  return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

double contrastRatio(const QColor& first, const QColor& second) {
  const double l1 = relativeLuminance(first);
  const double l2 = relativeLuminance(second);
  const double lighter = std::max(l1, l2);
  const double darker = std::min(l1, l2);
  return (lighter + 0.05) / (darker + 0.05);
}

QColor blend(const QColor& first, const QColor& second, double secondWeight) {
  const double w = std::clamp(secondWeight, 0.0, 1.0);
  const double inv = 1.0 - w;
  return QColor::fromRgbF(first.redF() * inv + second.redF() * w,
                          first.greenF() * inv + second.greenF() * w,
                          first.blueF() * inv + second.blueF() * w,
                          first.alphaF() * inv + second.alphaF() * w);
}

QString toHex(const QColor& color) {
  return color.name(QColor::HexRgb);
}

QColor readableCandidate(const QColor& background, const QColor& preferred) {
  const QColor light(QStringLiteral("#f8fafc"));
  const QColor dark(QStringLiteral("#0f172a"));
  const double preferredContrast = contrastRatio(preferred, background);
  const double lightContrast = contrastRatio(light, background);
  const double darkContrast = contrastRatio(dark, background);
  if (preferredContrast >= lightContrast && preferredContrast >= darkContrast) {
    return preferred;
  }
  return lightContrast >= darkContrast ? light : dark;
}

QColor ensureMinContrast(const QColor& foreground, const QColor& background,
                         double minContrast) {
  QColor result = foreground;
  if (contrastRatio(result, background) >= minContrast) {
    return result;
  }

  const QColor candidate = readableCandidate(background, foreground);
  if (contrastRatio(candidate, background) >= minContrast) {
    return candidate;
  }

  const QColor towardLight(QStringLiteral("#f8fafc"));
  const QColor towardDark(QStringLiteral("#0f172a"));
  const bool preferLight =
      contrastRatio(towardLight, background) >= contrastRatio(towardDark, background);
  const QColor anchor = preferLight ? towardLight : towardDark;
  for (int step = 1; step <= 10; ++step) {
    const double weight = static_cast<double>(step) / 10.0;
    const QColor mixed = blend(foreground, anchor, weight);
    if (contrastRatio(mixed, background) >= minContrast) {
      return mixed;
    }
  }
  return candidate;
}

ThemeSpec normalizedTheme(ThemeSpec input) {
  QColor window(input.windowBg);
  QColor surface(input.surface);
  QColor surfaceAlt(input.surfaceAlt);
  QColor accent(input.accent);
  QColor border(input.border);
  QColor headerBg(input.headerBg);

  if (!window.isValid()) window = QColor(QStringLiteral("#f6f8fb"));
  if (!surface.isValid()) surface = QColor(QStringLiteral("#ffffff"));
  if (!surfaceAlt.isValid()) surfaceAlt = blend(surface, window, 0.18);
  if (!accent.isValid()) accent = QColor(QStringLiteral("#0f8f96"));
  if (!border.isValid()) border = blend(surface, QColor(QStringLiteral("#0f172a")), 0.22);
  if (!headerBg.isValid()) headerBg = blend(window, accent, 0.58);

  const bool darkTheme = input.dark;
  if (headerBg.lightnessF() > 0.45) {
    headerBg = blend(headerBg, QColor(QStringLiteral("#0b1220")), darkTheme ? 0.34 : 0.50);
  }

  const QColor text =
      ensureMinContrast(QColor(input.text), window, 7.0);
  const QColor headerFg =
      ensureMinContrast(QColor(input.headerFg), headerBg, 7.0);
  const QColor accentText =
      ensureMinContrast(QColor(input.accentText), accent, 4.5);

  const QColor hover = blend(surface, accent, darkTheme ? 0.22 : 0.14);
  const QColor selection = blend(surface, accent, darkTheme ? 0.42 : 0.30);
  const QColor selectionText =
      ensureMinContrast(QColor(input.listSelectionText), selection, 7.0);
  const QColor separator = blend(border, text, darkTheme ? 0.30 : 0.20);
  const QColor disabled = blend(text, window, darkTheme ? 0.50 : 0.58);

  const QColor resolvedBorder = contrastRatio(border, surface) >= 2.0
                                    ? border
                                    : blend(surface, text, darkTheme ? 0.34 : 0.24);
  const QColor resolvedSurfaceAlt = blend(surfaceAlt, window, darkTheme ? 0.10 : 0.08);
  const QColor resolvedAltBase = blend(surface, window, darkTheme ? 0.22 : 0.10);

  input.windowBg = toHex(window);
  input.surface = toHex(surface);
  input.surfaceAlt = toHex(resolvedSurfaceAlt);
  input.text = toHex(text);
  input.accent = toHex(accent);
  input.accentText = toHex(accentText);
  input.headerBg = toHex(headerBg);
  input.headerFg = toHex(headerFg);
  input.border = toHex(resolvedBorder);
  input.hover = toHex(hover);
  input.listSelection = toHex(selection);
  input.listSelectionText = toHex(selectionText);
  input.separator = toHex(separator);
  input.disabledText = toHex(disabled);
  input.alternateBase = toHex(resolvedAltBase);
  return input;
}

ThemeSpec lightTheme() {
  ThemeSpec s;
  s.dark = false;
  s.windowBg = "#f6f8fb";
  s.text = "#0f172a";
  s.surface = "#ffffff";
  s.surfaceAlt = "#eef2f7";
  s.accent = "#0f8f96";
  s.border = "#d6dde8";
  s.headerBg = "#0f766e";
  s.headerFg = "#f8fafc";
  s.hover = "#e6edf6";
  s.listSelection = "rgba(15, 143, 150, 0.22)";
  s.separator = "#dde5f0";
  s.listSelectionText = "#0f172a";
  s.accentText = "#ffffff";
  s.disabledText = "#8b95a7";
  s.alternateBase = "#f1f5f9";
  return s;
}

ThemeSpec darkTheme() {
  ThemeSpec s;
  s.dark = true;
  s.windowBg = "#111827";
  s.text = "#e5e7eb";
  s.surface = "#1f2937";
  s.surfaceAlt = "#182232";
  s.accent = "#2dd4bf";
  s.border = "#374151";
  s.headerBg = "#0b1220";
  s.headerFg = "#e5e7eb";
  s.hover = "#2a3648";
  s.listSelection = "rgba(45, 212, 191, 0.30)";
  s.separator = "#334155";
  s.listSelectionText = "#f8fafc";
  s.accentText = "#022c22";
  s.disabledText = "#6b7280";
  s.alternateBase = "#1a2433";
  return s;
}

ThemeSpec arduinoTheme() {
  ThemeSpec s;
  s.dark = false;
  s.windowBg = "#ffffff";
  s.text = "#111827";
  s.surface = "#ffffff";
  s.surfaceAlt = "#f3f4f6";
  s.accent = "#00979c";
  s.border = "#d1d5db";
  s.headerBg = "#00878f";
  s.headerFg = "#ffffff";
  s.hover = "#eef2f7";
  s.listSelection = "rgba(0, 151, 156, 0.24)";
  s.separator = "#e5e7eb";
  s.listSelectionText = "#111827";
  s.accentText = "#ffffff";
  s.disabledText = "#6b7280";
  s.alternateBase = "#f8fafc";
  return s;
}

ThemeSpec oceanicTheme() {
  ThemeSpec s;
  s.dark = true;
  s.windowBg = "#0b1220";
  s.text = "#dce6f4";
  s.surface = "#111b2f";
  s.surfaceAlt = "#0f1a2b";
  s.accent = "#4ecdc4";
  s.border = "#2f3f59";
  s.headerBg = "#11203a";
  s.headerFg = "#dce6f4";
  s.hover = "#162640";
  s.listSelection = "rgba(78, 205, 196, 0.28)";
  s.separator = "#2a3a54";
  s.listSelectionText = "#f3f7ff";
  s.accentText = "#022624";
  s.disabledText = "#7b8ca6";
  s.alternateBase = "#102036";
  return s;
}

ThemeSpec cyberTheme() {
  ThemeSpec s;
  s.dark = true;
  s.windowBg = "#0a0e17";
  s.text = "#d8e0ff";
  s.surface = "#10192b";
  s.surfaceAlt = "#0d1524";
  s.accent = "#00d9ff";
  s.border = "#24304a";
  s.headerBg = "#111d38";
  s.headerFg = "#c6f6ff";
  s.hover = "#19253f";
  s.listSelection = "rgba(0, 217, 255, 0.24)";
  s.separator = "#2b3957";
  s.listSelectionText = "#f8fcff";
  s.accentText = "#001a22";
  s.disabledText = "#70809d";
  s.alternateBase = "#10192a";
  return s;
}

ThemeSpec y2kTheme() {
  ThemeSpec s;
  s.dark = false;
  s.windowBg = "#fff7ff";
  s.text = "#31163f";
  s.surface = "#ffffff";
  s.surfaceAlt = "#f9e6ff";
  s.accent = "#c026d3";
  s.border = "#ebc4ff";
  s.headerBg = "#a21caf";
  s.headerFg = "#ffffff";
  s.hover = "#f4ddff";
  s.listSelection = "rgba(192, 38, 211, 0.20)";
  s.separator = "#efddfb";
  s.listSelectionText = "#31163f";
  s.accentText = "#ffffff";
  s.disabledText = "#9b7cb3";
  s.alternateBase = "#ffeffc";
  return s;
}

ThemeSpec graphiteTheme() {
  ThemeSpec s;
  s.dark = true;
  s.windowBg = "#0f1115";
  s.text = "#e5e7eb";
  s.surface = "#171a20";
  s.surfaceAlt = "#141820";
  s.accent = "#60a5fa";
  s.border = "#2a2f3a";
  s.headerBg = "#151b27";
  s.headerFg = "#e6edf7";
  s.hover = "#202734";
  s.listSelection = "rgba(96, 165, 250, 0.26)";
  s.separator = "#313845";
  s.listSelectionText = "#f8fafc";
  s.accentText = "#081b33";
  s.disabledText = "#707786";
  s.alternateBase = "#131923";
  return s;
}

ThemeSpec nordTheme() {
  ThemeSpec s;
  s.dark = true;
  s.windowBg = "#2e3440";
  s.text = "#e5e9f0";
  s.surface = "#3b4252";
  s.surfaceAlt = "#353c4a";
  s.accent = "#88c0d0";
  s.border = "#4c566a";
  s.headerBg = "#3a4254";
  s.headerFg = "#eceff4";
  s.hover = "#434c5e";
  s.listSelection = "rgba(136, 192, 208, 0.30)";
  s.separator = "#566178";
  s.listSelectionText = "#f2f4f8";
  s.accentText = "#0f2a33";
  s.disabledText = "#8f9bb2";
  s.alternateBase = "#38404f";
  return s;
}

ThemeSpec everforestTheme() {
  ThemeSpec s;
  s.dark = true;
  s.windowBg = "#232a2e";
  s.text = "#d3c6aa";
  s.surface = "#2d353b";
  s.surfaceAlt = "#283035";
  s.accent = "#a7c080";
  s.border = "#4f5b58";
  s.headerBg = "#343f44";
  s.headerFg = "#e6dfc8";
  s.hover = "#3a454a";
  s.listSelection = "rgba(167, 192, 128, 0.28)";
  s.separator = "#56635f";
  s.listSelectionText = "#f0ead6";
  s.accentText = "#1d2a14";
  s.disabledText = "#88908a";
  s.alternateBase = "#2a3236";
  return s;
}

ThemeSpec dawnTheme() {
  ThemeSpec s;
  s.dark = false;
  s.windowBg = "#f8fafc";
  s.text = "#0f172a";
  s.surface = "#ffffff";
  s.surfaceAlt = "#eef2ff";
  s.accent = "#4f46e5";
  s.border = "#d1d9e6";
  s.headerBg = "#312e81";
  s.headerFg = "#eef2ff";
  s.hover = "#e5eaf8";
  s.listSelection = "rgba(79, 70, 229, 0.20)";
  s.separator = "#dbe2f0";
  s.listSelectionText = "#0f172a";
  s.accentText = "#ffffff";
  s.disabledText = "#7c8596";
  s.alternateBase = "#f1f5f9";
  return s;
}

ThemeSpec auroraTheme() {
  ThemeSpec s;
  s.dark = false;
  s.windowBg = "#f5f8ff";
  s.text = "#0f1f3d";
  s.surface = "#ffffff";
  s.surfaceAlt = "#e9eefc";
  s.accent = "#2563eb";
  s.border = "#cfd8ef";
  s.headerBg = "#1d4ed8";
  s.headerFg = "#f8fbff";
  s.hover = "#e4ebff";
  s.listSelection = "rgba(37, 99, 235, 0.24)";
  s.separator = "#d7e0f4";
  s.listSelectionText = "#0b1730";
  s.accentText = "#ffffff";
  s.disabledText = "#7c8aa6";
  s.alternateBase = "#edf3ff";
  return s;
}

ThemeSpec midnightTheme() {
  ThemeSpec s;
  s.dark = true;
  s.windowBg = "#070f1f";
  s.text = "#dbeafe";
  s.surface = "#0d172b";
  s.surfaceAlt = "#0b1426";
  s.accent = "#38bdf8";
  s.border = "#24324a";
  s.headerBg = "#0a1428";
  s.headerFg = "#e0f2fe";
  s.hover = "#15233c";
  s.listSelection = "rgba(56, 189, 248, 0.26)";
  s.separator = "#2a3955";
  s.listSelectionText = "#ecfeff";
  s.accentText = "#05263a";
  s.disabledText = "#6c7a93";
  s.alternateBase = "#111d32";
  return s;
}

ThemeSpec terraTheme() {
  ThemeSpec s;
  s.dark = true;
  s.windowBg = "#16120f";
  s.text = "#f3e7db";
  s.surface = "#211a15";
  s.surfaceAlt = "#1d1612";
  s.accent = "#f97316";
  s.border = "#4a372a";
  s.headerBg = "#2a1f18";
  s.headerFg = "#fff1e5";
  s.hover = "#2e231c";
  s.listSelection = "rgba(249, 115, 22, 0.28)";
  s.separator = "#584235";
  s.listSelectionText = "#fff7ef";
  s.accentText = "#2f1303";
  s.disabledText = "#9f8b7b";
  s.alternateBase = "#241c17";
  return s;
}

ThemeSpec resolveTheme(QString theme, bool systemDark, bool* ok) {
  theme = theme.trimmed().toLower();
  if (theme.isEmpty()) {
    theme = QStringLiteral("system");
  }
  if (theme == QStringLiteral("system")) {
    theme = systemDark ? QStringLiteral("dark") : QStringLiteral("light");
  }

  if (theme == QStringLiteral("light")) {
    if (ok) *ok = true;
    return lightTheme();
  }
  if (theme == QStringLiteral("dark")) {
    if (ok) *ok = true;
    return darkTheme();
  }
  if (theme == QStringLiteral("arduino")) {
    if (ok) *ok = true;
    return arduinoTheme();
  }
  if (theme == QStringLiteral("oceanic")) {
    if (ok) *ok = true;
    return oceanicTheme();
  }
  if (theme == QStringLiteral("cyber")) {
    if (ok) *ok = true;
    return cyberTheme();
  }
  if (theme == QStringLiteral("y2k")) {
    if (ok) *ok = true;
    return y2kTheme();
  }
  if (theme == QStringLiteral("graphite")) {
    if (ok) *ok = true;
    return graphiteTheme();
  }
  if (theme == QStringLiteral("nord")) {
    if (ok) *ok = true;
    return nordTheme();
  }
  if (theme == QStringLiteral("everforest")) {
    if (ok) *ok = true;
    return everforestTheme();
  }
  if (theme == QStringLiteral("dawn")) {
    if (ok) *ok = true;
    return dawnTheme();
  }
  if (theme == QStringLiteral("aurora")) {
    if (ok) *ok = true;
    return auroraTheme();
  }
  if (theme == QStringLiteral("midnight")) {
    if (ok) *ok = true;
    return midnightTheme();
  }
  if (theme == QStringLiteral("terra")) {
    if (ok) *ok = true;
    return terraTheme();
  }

  if (ok) *ok = false;
  return {};
}

QString buildStyleSheet(const ThemeSpec& t) {
  const QColor headerBg(t.headerBg);
  const QColor headerFg(t.headerFg);
  const QColor accent(t.accent);
  const QColor text(t.text);
  const QColor window(t.windowBg);

  const QColor menuItemHover = blend(headerBg, headerFg, 0.22);
  const QColor menuItemPressed = blend(headerBg, headerFg, 0.32);
  const QColor headerButtonHover = blend(headerBg, headerFg, 0.18);
  const QColor headerButtonPressed = blend(headerBg, headerFg, 0.28);
  const QColor headerButtonChecked = blend(headerBg, headerFg, 0.24);
  const QColor headerComboBackground = blend(headerBg, headerFg, 0.16);
  const QColor headerComboBorder = blend(headerBg, headerFg, 0.35);
  const QColor headerComboHover = blend(headerBg, headerFg, 0.24);
  const QColor statusProgressTrack = blend(headerBg, window, t.dark ? 0.55 : 0.40);

  const QString headerArrowIcon =
      relativeLuminance(headerFg) >= 0.42
          ? QStringLiteral(":/icons/arrow-down.png")
          : QStringLiteral(":/icons/arrow-down-light.png");
  const QString comboArrowIcon =
      relativeLuminance(text) >= 0.42
          ? QStringLiteral(":/icons/arrow-down.png")
          : QStringLiteral(":/icons/arrow-down-light.png");
  const QColor checkMarkColor =
      ensureMinContrast(QColor(t.accentText), accent, 4.5);
  const QString checkIcon =
      relativeLuminance(checkMarkColor) >= 0.42
          ? QStringLiteral(":/icons/check-dark.svg")
          : QStringLiteral(":/icons/check.png");

  return QString(R"(
    QWidget {
        background-color: %1;
        color: %2;
        font-family: "Segoe UI", "Inter", "Roboto", "Fira Sans", sans-serif;
        outline: none;
    }

    QMainWindow {
        background-color: %1;
    }

    QAbstractItemView {
        background-color: %3;
        color: %2;
        alternate-background-color: %15;
        selection-background-color: %10;
        selection-color: %12;
    }

    QMainWindow::separator {
        background: %11;
        width: 1px;
        height: 1px;
    }

    QMainWindow::separator:hover {
        background: %5;
        width: 3px;
        height: 3px;
    }

    QMenuBar {
        background-color: %7;
        background-image: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                          stop:0 %7, stop:1 %4);
        color: %8;
        border: none;
        border-bottom: 1px solid %6;
        padding: 2px 4px;
    }

    QMenuBar::item {
        background: transparent;
        color: %8;
        padding: 6px 10px;
        border-radius: 4px;
    }

    QMenuBar::item:selected {
        background-color: %16;
    }

    QMenuBar::item:pressed {
        background-color: %17;
    }

    QToolBar#HeaderToolBar {
        background-color: %7;
        background-image: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                          stop:0 %7, stop:1 %4);
        border: none;
        border-bottom: 1px solid %6;
        padding: 6px 8px;
        spacing: 6px;
    }

    QToolBar#HeaderToolBar QToolButton {
        background: transparent;
        border: none;
        border-radius: 4px;
        padding: 6px 10px;
        color: %8;
        font-weight: 500;
    }

    QToolBar#HeaderToolBar QToolButton:hover {
        background-color: %18;
    }

    QToolBar#HeaderToolBar QToolButton:pressed {
        background-color: %19;
    }

    QToolBar#HeaderToolBar QToolButton:checked {
        background-color: %20;
    }

    QToolBar#HeaderToolBar QLabel {
        color: %8;
        font-weight: 600;
        margin-left: 4px;
        margin-right: 4px;
    }

    QToolBar#HeaderToolBar QWidget {
        background: transparent;
        color: %8;
    }

    QToolBar#HeaderToolBar QComboBox {
        background-color: %21;
        border: 1px solid %22;
        border-radius: 4px;
        padding: 4px 24px 4px 8px;
        color: %8;
        min-height: 20px;
    }

    QToolBar#HeaderToolBar QComboBox:hover {
        background-color: %23;
        border-color: %24;
    }

    QToolBar#HeaderToolBar QComboBox::drop-down {
        border: none;
        width: 24px;
        padding-right: 4px;
        subcontrol-origin: padding;
        subcontrol-position: right top;
        position: absolute;
        top: 2px;
        bottom: 2px;
        right: 2px;
    }

    QToolBar#HeaderToolBar QComboBox::down-arrow {
        image: url(%25);
        width: 10px;
        height: 10px;
    }

    QToolBar#HeaderToolBar QComboBox QAbstractItemView {
        background-color: %3;
        border: 1px solid %6;
        border-radius: 6px;
        color: %2;
        selection-background-color: %5;
        selection-color: %12;
    }

    QToolBar#HeaderToolBar QComboBox QAbstractItemView::item {
        color: %2;
        padding: 6px 8px;
        min-height: 22px;
    }

    QToolBar#HeaderToolBar QComboBox QAbstractItemView::item:selected {
        background-color: %5;
        color: %12;
    }

    QToolBar#ActivityBar {
        background-color: %4;
        border: none;
        border-right: 1px solid %11;
        spacing: 0px;
        min-width: 48px;
        max-width: 48px;
    }

    QToolBar#ActivityBar QToolButton {
        background: transparent;
        border: none;
        padding: 12px;
        border-left: 3px solid transparent;
        icon-size: 24px;
    }

    QToolBar#ActivityBar QToolButton:hover {
        background-color: %9;
    }

    QToolBar#ActivityBar QToolButton:checked {
        background-color: %10;
        border-left: 3px solid %5;
    }

    QDockWidget {
        border: none;
        background-color: %3;
    }

    QDockWidget::title {
        background-color: %4;
        color: %2;
        font-weight: 600;
        padding: 8px 12px;
        border-bottom: 1px solid %11;
    }

    QDockWidget::close-button, QDockWidget::float-button {
        background: transparent;
        border: none;
        border-radius: 4px;
        min-width: 24px;
        min-height: 24px;
    }

    QDockWidget::close-button:hover, QDockWidget::float-button:hover {
        background: %9;
    }

    QTabWidget::pane {
        border: none;
        border-top: 1px solid %11;
        background-color: %3;
    }

    QTabBar::tab {
        background-color: transparent;
        color: %2;
        padding: 9px 14px;
        border: none;
        border-right: 1px solid %11;
        min-width: 96px;
    }

    QTabBar::tab:selected {
        background-color: %1;
        color: %2;
        border-top: 3px solid %5;
        font-weight: 600;
    }

    QTabBar::tab:hover:!selected {
        background-color: %9;
    }

    QTreeView, QListView {
        background-color: %3;
        border: none;
        alternate-background-color: %15;
        selection-background-color: %10;
        selection-color: %12;
    }

    QTreeView::item, QListView::item {
        padding: 6px 8px;
        min-height: 24px;
    }

    QTreeView::item:hover, QListView::item:hover {
        background-color: %9;
    }

    QTreeView::item:selected, QListView::item:selected {
        background-color: %10;
        color: %12;
    }

    QScrollBar:vertical {
        background: transparent;
        width: 14px;
    }

    QScrollBar::handle:vertical {
        background: rgba(128, 128, 128, 0.45);
        min-height: 30px;
        border-radius: 7px;
        margin: 2px;
    }

    QScrollBar::handle:vertical:hover {
        background: rgba(128, 128, 128, 0.62);
    }

    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
        height: 0px;
        background: none;
    }

    QScrollBar:horizontal {
        background: transparent;
        height: 14px;
    }

    QScrollBar::handle:horizontal {
        background: rgba(128, 128, 128, 0.45);
        min-width: 30px;
        border-radius: 7px;
        margin: 2px;
    }

    QScrollBar::handle:horizontal:hover {
        background: rgba(128, 128, 128, 0.62);
    }

    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
    QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
        width: 0px;
        background: none;
    }

    QPlainTextEdit#OutputTextEdit {
        background-color: %1;
        color: %2;
        border: none;
        font-family: "JetBrains Mono", "Cascadia Code", "Consolas", "Menlo", monospace;
        font-size: 12px;
        selection-background-color: %5;
        selection-color: %12;
    }

    QPlainTextEdit {
        selection-background-color: %5;
        selection-color: %13;
    }

    QStatusBar {
        background-color: %7;
        background-image: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                          stop:0 %7, stop:1 %4);
        color: %8;
        border: none;
        border-top: 1px solid rgba(255, 255, 255, 0.10);
        padding: 2px 4px;
    }

    QStatusBar QLabel {
        background: transparent;
        color: %8;
        padding: 0 8px;
    }

    QStatusBar QProgressBar {
        background-color: %26;
        border: none;
        border-radius: 3px;
        height: 16px;
        text-align: center;
    }

    QStatusBar QProgressBar::chunk {
        background-color: %8;
        border-radius: 2px;
    }

    QComboBox {
        background-color: %3;
        border: 1px solid %6;
        border-radius: 4px;
        padding: 4px 24px 4px 8px;
        color: %2;
        min-height: 22px;
    }

    QComboBox:hover {
        border-color: %5;
    }

    QComboBox::drop-down {
        border: none;
        width: 24px;
        padding-right: 4px;
        subcontrol-origin: padding;
        subcontrol-position: right top;
        position: absolute;
        top: 2px;
        bottom: 2px;
        right: 2px;
    }

    QComboBox::down-arrow {
        image: url(%27);
        width: 10px;
        height: 10px;
    }

    QComboBox QAbstractItemView {
        background-color: %3;
        color: %2;
        border: 1px solid %6;
        border-radius: 6px;
        selection-background-color: %5;
        selection-color: %13;
    }

    QComboBox QAbstractItemView::item {
        color: %2;
        padding: 6px 8px;
        min-height: 22px;
    }

    QComboBox QAbstractItemView::item:selected {
        background-color: %5;
        color: %12;
    }

    QPushButton {
        background-color: %5;
        color: %12;
        border: none;
        border-radius: 6px;
        padding: 6px 16px;
        min-height: 24px;
        font-weight: 500;
    }

    QPushButton:focus {
        border: 2px solid %8;
        padding: 5px 15px;
    }

    QPushButton:hover {
        background-color: %7;
    }

    QPushButton:pressed {
        background-color: %7;
    }

    QPushButton:disabled {
        background-color: %9;
        color: %14;
    }

    QLineEdit {
        background-color: %3;
        border: 1px solid %6;
        border-radius: 4px;
        padding: 4px 8px;
        color: %2;
        selection-background-color: %5;
        selection-color: %13;
    }

    QLineEdit:focus {
        border: 2px solid %5;
        padding: 3px 7px;
    }

    QMenu {
        background-color: %3;
        color: %2;
        border: 1px solid %6;
        border-radius: 8px;
        padding: 4px;
    }

    QMenu::item {
        color: %2;
        padding: 8px 24px;
        border-radius: 4px;
    }

    QMenu::item:selected {
        background-color: %5;
        color: %13;
    }

    QMenu::item:disabled {
        color: %14;
        background-color: transparent;
    }

    QMenu::separator {
        height: 1px;
        background: %11;
        margin: 4px 8px;
    }

    QToolTip {
        background-color: %1;
        color: %2;
        border: 1px solid %6;
        padding: 4px 8px;
        border-radius: 4px;
    }

    QGroupBox {
        border: 1px solid %6;
        border-radius: 6px;
        margin-top: 12px;
        padding-top: 8px;
        font-weight: 600;
    }

    QGroupBox::title {
        subcontrol-origin: margin;
        left: 12px;
        padding: 0 6px;
    }

    QTableView {
        background-color: %3;
        alternate-background-color: %15;
        gridline-color: %11;
        selection-background-color: %5;
        selection-color: %13;
    }

    QTableView::item:selected {
        background-color: %5;
        color: %13;
    }

    QTableView QHeaderView::section {
        background-color: %4;
        color: %2;
        padding: 6px 8px;
        border: none;
        border-right: 1px solid %11;
        border-bottom: 1px solid %11;
        font-weight: 600;
    }

    QCheckBox::indicator {
        width: 18px;
        height: 18px;
        border: 2px solid %6;
        border-radius: 3px;
        background-color: %3;
    }

    QCheckBox::indicator:checked {
        background-color: %5;
        border-color: %5;
        image: url(%28);
    }

    QRadioButton::indicator {
        width: 18px;
        height: 18px;
        border: 2px solid %6;
        border-radius: 9px;
        background-color: %3;
    }

    QRadioButton::indicator:checked {
        background-color: %5;
        border-color: %5;
    }

    QSpinBox, QDoubleSpinBox {
        background-color: %3;
        border: 1px solid %6;
        border-radius: 4px;
        padding: 4px 8px;
        color: %2;
    }

    QSpinBox:focus, QDoubleSpinBox:focus {
        border: 2px solid %5;
    }

    QSlider::groove:horizontal {
        height: 6px;
        background: %4;
        border-radius: 3px;
    }

    QSlider::handle:horizontal {
        width: 16px;
        height: 16px;
        background: %5;
        border-radius: 8px;
        margin: -5px 0;
    }

    QWidget#WelcomeWidget {
        background-color: %1;
    }

    QWidget#WelcomeWidget QLabel {
        color: %2;
        background: transparent;
    }

    QWidget#WelcomeWidget QGroupBox {
        border: 1px solid %6;
        border-radius: 8px;
        margin-top: 12px;
        padding-top: 16px;
        background-color: %3;
    }

    QWidget#WelcomeWidget QListWidget {
        background-color: %1;
        border: none;
        border-radius: 4px;
    }

    QWidget#WelcomeWidget QListWidget::item {
        padding: 10px 12px;
        border-radius: 4px;
        margin: 2px;
    }

    QWidget#WelcomeWidget QListWidget::item:hover {
        background-color: %9;
    }

    QWidget#WelcomeWidget QListWidget::item:selected {
        background-color: %5;
        color: %13;
    }

    QWidget#WelcomeWidget QPushButton {
        background-color: %5;
        color: %13;
        border-radius: 6px;
        padding: 10px 20px;
        min-width: 120px;
    }

    QWidget#ToastWidget {
        background-color: %3;
        color: %2;
        border: 1px solid %6;
        border-radius: 10px;
    }

    QWidget#ToastWidget[platform="windows"] {
        background-color: %1;
    }

    QWidget#ToastWidget QLabel#toastLabel {
        color: %2;
        background: transparent;
    }

    QWidget#ToastWidget QPushButton#toastActionButton {
        background-color: %5;
        color: %13;
        border: none;
        border-radius: 6px;
        padding: 6px 12px;
        min-width: 70px;
        font-weight: 600;
    }

    QWidget#ToastWidget QPushButton#toastActionButton:hover {
        background-color: %9;
        color: %2;
    }

    QWidget#ToastWidget QPushButton#toastActionButton:pressed {
        background-color: %10;
        color: %13;
    }

    QWidget#ToastWidget QToolButton#toastCloseButton {
        color: %2;
        border: none;
        border-radius: 4px;
        background: transparent;
        padding: 2px;
    }

    QWidget#ToastWidget QToolButton#toastCloseButton:hover {
        background-color: %9;
    }

    QWidget#OutputPanel {
        background-color: %1;
        border: none;
    }

    QComboBox:disabled, QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled {
        color: %14;
    }

    QTreeView::item:disabled, QListView::item:disabled, QTableView::item:disabled {
        color: %14;
    }
  )")
      .arg(t.windowBg, t.text, t.surface, t.surfaceAlt, t.accent, t.border,
           t.headerBg, t.headerFg, t.hover, t.listSelection, t.separator,
           t.listSelectionText, t.accentText, t.disabledText, t.alternateBase,
           toHex(menuItemHover), toHex(menuItemPressed), toHex(headerButtonHover),
           toHex(headerButtonPressed), toHex(headerButtonChecked),
           toHex(headerComboBackground), toHex(headerComboBorder),
           toHex(headerComboHover), toHex(blend(headerComboBorder, headerFg, 0.35)),
           headerArrowIcon, toHex(statusProgressTrack), comboArrowIcon, checkIcon);
}

QPalette buildPalette(const ThemeSpec& t) {
  const QColor windowColor(t.windowBg);
  const QColor textColor(t.text);
  const QColor surfaceColor(t.surface);
  const QColor altBaseColor(t.alternateBase);
  const QColor accentColor(t.accent);
  const QColor accentTextColor(t.accentText);
  const QColor disabledColor(t.disabledText);

  QPalette p;
  p.setColor(QPalette::Window, windowColor);
  p.setColor(QPalette::WindowText, textColor);
  p.setColor(QPalette::Base, surfaceColor);
  p.setColor(QPalette::AlternateBase, altBaseColor);
  p.setColor(QPalette::ToolTipBase, surfaceColor);
  p.setColor(QPalette::ToolTipText, textColor);
  p.setColor(QPalette::Text, textColor);
  p.setColor(QPalette::Button, surfaceColor);
  p.setColor(QPalette::ButtonText, textColor);
  p.setColor(QPalette::BrightText, QColor("#ff4d4f"));
  p.setColor(QPalette::Link, accentColor);
  p.setColor(QPalette::Highlight, accentColor);
  p.setColor(QPalette::HighlightedText, accentTextColor);
  p.setColor(QPalette::PlaceholderText, disabledColor);

  p.setColor(QPalette::Disabled, QPalette::WindowText, disabledColor);
  p.setColor(QPalette::Disabled, QPalette::Text, disabledColor);
  p.setColor(QPalette::Disabled, QPalette::ButtonText, disabledColor);
  p.setColor(QPalette::Disabled, QPalette::PlaceholderText, disabledColor);

  return p;
}
}  // namespace

void ThemeManager::init() {
  if (g_inited) {
    return;
  }
  g_inited = true;
  g_defaultPalette = QApplication::palette();

  const QString styleName =
      QApplication::style() ? QApplication::style()->objectName() : QString{};
  g_defaultStyleKey = normalizeStyleKey(styleName);
}

void ThemeManager::apply(const QString& theme) {
  init();

  bool systemDark = false;
  if (QGuiApplication::styleHints()) {
    systemDark =
        QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
  }

  bool ok = false;
  ThemeSpec spec = resolveTheme(theme, systemDark, &ok);
  if (!ok) {
    qApp->setStyleSheet(QString{});
    if (!g_defaultStyleKey.isEmpty()) {
      if (QStyle* style = QStyleFactory::create(g_defaultStyleKey)) {
        QApplication::setStyle(style);
      }
    }
    QApplication::setPalette(g_defaultPalette);
    return;
  }

  spec = normalizedTheme(spec);

  QApplication::setStyle(QStyleFactory::create("Fusion"));
  QApplication::setPalette(buildPalette(spec));
  qApp->setStyleSheet(buildStyleSheet(spec));
}

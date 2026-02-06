#include "interface_scale_manager.h"

#include <algorithm>
#include <QApplication>
#include <QFont>

namespace {
QFont g_baseFont;
double g_currentScale = 1.0;
bool g_initialized = false;

double clampScale(double scale) {
  if (!(scale > 0.0)) {
    return 1.0;
  }
  return std::clamp(scale, 0.5, 2.0);
}

QFont scaledFont(double scale) {
  QFont f = g_baseFont;

  const double base = g_baseFont.pointSizeF() > 0 ? g_baseFont.pointSizeF()
                                                  : static_cast<double>(g_baseFont.pointSize());
  if (base > 0.0) {
    f.setPointSizeF(base * scale);
  }
  return f;
}
}  // namespace

void UiScaleManager::init() {
  if (g_initialized) {
    return;
  }
  g_baseFont = QApplication::font();
  g_currentScale = 1.0;
  g_initialized = true;
}

double UiScaleManager::currentScale() {
  return g_currentScale;
}

void UiScaleManager::apply(double scale) {
  if (!g_initialized) {
    init();
  }
  const double clamped = clampScale(scale);
  g_currentScale = clamped;
  QApplication::setFont(scaledFont(clamped));
}

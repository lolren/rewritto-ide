#include "serial_plot_range.h"

#include <algorithm>
#include <cmath>

SerialPlotYRange serialPlotComputeAutoRange(const QVector<QVector<double>>& series) {
  SerialPlotYRange out;
  out.hasValue = false;
  out.minY = 0.0;
  out.maxY = 1.0;

  for (const auto& s : series) {
    for (double v : s) {
      if (std::isnan(v)) {
        continue;
      }
      if (!out.hasValue) {
        out.minY = out.maxY = v;
        out.hasValue = true;
      } else {
        out.minY = std::min(out.minY, v);
        out.maxY = std::max(out.maxY, v);
      }
    }
  }
  return out;
}

SerialPlotYRange serialPlotNormalizeRange(SerialPlotYRange range) {
  if (!range.hasValue) {
    return range;
  }
  if (range.minY > range.maxY) {
    std::swap(range.minY, range.maxY);
  }
  if (range.minY == range.maxY) {
    range.minY -= 1.0;
    range.maxY += 1.0;
  }
  return range;
}

void SerialPlotRangeController::setAutoScaleEnabled(bool enabled) {
  if (autoScaleEnabled_ == enabled) {
    return;
  }
  autoScaleEnabled_ = enabled;
  if (!autoScaleEnabled_) {
    freezeEnabled_ = false;
    hasFrozenRange_ = false;
  }
}

bool SerialPlotRangeController::autoScaleEnabled() const {
  return autoScaleEnabled_;
}

void SerialPlotRangeController::setFreezeEnabled(bool enabled) {
  if (!autoScaleEnabled_) {
    freezeEnabled_ = false;
    hasFrozenRange_ = false;
    return;
  }
  if (freezeEnabled_ == enabled) {
    return;
  }

  freezeEnabled_ = enabled;
  if (!freezeEnabled_) {
    hasFrozenRange_ = false;
    frozenRange_ = SerialPlotYRange{};
    return;
  }

  if (hasAutoRange_) {
    frozenRange_ = lastAutoRange_;
    hasFrozenRange_ = true;
  } else {
    hasFrozenRange_ = false;
  }
}

bool SerialPlotRangeController::freezeEnabled() const {
  return freezeEnabled_;
}

void SerialPlotRangeController::setManualRange(double minY, double maxY) {
  manualMinY_ = minY;
  manualMaxY_ = maxY;
}

double SerialPlotRangeController::manualMinY() const {
  return manualMinY_;
}

double SerialPlotRangeController::manualMaxY() const {
  return manualMaxY_;
}

void SerialPlotRangeController::updateAutoRange(SerialPlotYRange range) {
  range = serialPlotNormalizeRange(range);
  if (!range.hasValue) {
    return;
  }
  lastAutoRange_ = range;
  hasAutoRange_ = true;
  if (!freezeEnabled_ || !hasFrozenRange_) {
    return;
  }
}

SerialPlotYRange SerialPlotRangeController::currentRange() const {
  if (!autoScaleEnabled_) {
    SerialPlotYRange out;
    out.hasValue = true;
    out.minY = manualMinY_;
    out.maxY = manualMaxY_;
    return serialPlotNormalizeRange(out);
  }

  if (freezeEnabled_ && hasFrozenRange_) {
    SerialPlotYRange out = frozenRange_;
    out.hasValue = true;
    return serialPlotNormalizeRange(out);
  }

  if (hasAutoRange_) {
    SerialPlotYRange out = lastAutoRange_;
    out.hasValue = true;
    return serialPlotNormalizeRange(out);
  }

  SerialPlotYRange out;
  out.hasValue = true;
  out.minY = 0.0;
  out.maxY = 1.0;
  return out;
}


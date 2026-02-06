#pragma once

#include <QVector>

struct SerialPlotYRange final {
  bool hasValue = false;
  double minY = 0.0;
  double maxY = 1.0;
};

SerialPlotYRange serialPlotComputeAutoRange(const QVector<QVector<double>>& series);
SerialPlotYRange serialPlotNormalizeRange(SerialPlotYRange range);

class SerialPlotRangeController final {
 public:
  void setAutoScaleEnabled(bool enabled);
  bool autoScaleEnabled() const;

  void setFreezeEnabled(bool enabled);
  bool freezeEnabled() const;

  void setManualRange(double minY, double maxY);
  double manualMinY() const;
  double manualMaxY() const;

  void updateAutoRange(SerialPlotYRange range);
  SerialPlotYRange currentRange() const;

 private:
  bool autoScaleEnabled_ = true;
  bool freezeEnabled_ = false;

  SerialPlotYRange lastAutoRange_;
  bool hasAutoRange_ = false;

  SerialPlotYRange frozenRange_;
  bool hasFrozenRange_ = false;

  double manualMinY_ = 0.0;
  double manualMaxY_ = 1.0;
};


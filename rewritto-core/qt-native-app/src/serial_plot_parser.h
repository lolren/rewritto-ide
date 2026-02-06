#pragma once

#include <QStringList>
#include <QVector>

class QString;

struct SerialPlotSample final {
  QStringList labels;
  QVector<double> values;
};

class SerialPlotParser final {
 public:
  QVector<double> parseLine(const QString& line) const;
  SerialPlotSample parseSample(const QString& line) const;
};

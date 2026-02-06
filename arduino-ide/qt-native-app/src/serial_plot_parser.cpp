#include "serial_plot_parser.h"

#include <QRegularExpression>
#include <QString>

QVector<double> SerialPlotParser::parseLine(const QString& line) const {
  static const QRegularExpression re(
      R"([-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)");

  QVector<double> out;
  auto it = re.globalMatch(line);
  while (it.hasNext()) {
    const QRegularExpressionMatch m = it.next();
    bool ok = false;
    const double v = m.captured(0).toDouble(&ok);
    if (ok) {
      out.push_back(v);
    }
  }
  return out;
}

SerialPlotSample SerialPlotParser::parseSample(const QString& line) const {
  SerialPlotSample sample;
  const QString text = line.trimmed();
  if (text.isEmpty()) {
    return sample;
  }

  static const QRegularExpression labeledRe(
      R"(([A-Za-z_][A-Za-z0-9_\-]*)\s*[:=]\s*([-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?))");

  auto it = labeledRe.globalMatch(text);
  while (it.hasNext()) {
    const QRegularExpressionMatch m = it.next();
    const QString label = m.captured(1).trimmed();
    bool ok = false;
    const double v = m.captured(2).toDouble(&ok);
    if (!ok || label.isEmpty()) {
      continue;
    }
    sample.labels.push_back(label);
    sample.values.push_back(v);
  }

  if (!sample.labels.isEmpty() && sample.labels.size() == sample.values.size()) {
    return sample;
  }

  sample.labels.clear();
  sample.values = parseLine(text);
  return sample;
}

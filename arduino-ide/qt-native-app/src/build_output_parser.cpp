#include "build_output_parser.h"

#include <QRegularExpression>

namespace {
qint64 parseInt64(QString s) {
  s.remove(QLatin1Char(','));
  bool ok = false;
  const qint64 v = s.trimmed().toLongLong(&ok);
  return ok ? v : 0;
}
}  // namespace

QString BuildSizeSummary::toStatusText() const {
  QStringList parts;
  if (hasProgram) {
    parts << QStringLiteral("Flash %1 B (%2%)")
                 .arg(QString::number(programUsedBytes))
                 .arg(programUsedPct);
  }
  if (hasRam) {
    parts << QStringLiteral("RAM %1 B (%2%)")
                 .arg(QString::number(ramUsedBytes))
                 .arg(ramUsedPct);
  }
  return parts.join(QStringLiteral(" | "));
}

BuildSizeSummary parseBuildSizeSummary(const QString& output) {
  BuildSizeSummary summary;
  const QString text = output;

  static const QRegularExpression programRe(
      R"(Sketch uses\s+([0-9][0-9,]*)\s+bytes\s+\((\d+)%\)\s+of\s+program\s+storage\s+space\.?\s+Maximum\s+is\s+([0-9][0-9,]*)\s+bytes\.?)",
      QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression ramRe(
      R"(Global variables use\s+([0-9][0-9,]*)\s+bytes\s+\((\d+)%\)\s+of\s+dynamic\s+memory,\s+leaving\s+([0-9][0-9,]*)\s+bytes\s+for\s+local\s+variables\.?\s+Maximum\s+is\s+([0-9][0-9,]*)\s+bytes\.?)",
      QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);

  for (auto it = programRe.globalMatch(text); it.hasNext();) {
    const QRegularExpressionMatch m = it.next();
    summary.hasProgram = true;
    summary.programUsedBytes = parseInt64(m.captured(1));
    summary.programUsedPct = m.captured(2).toInt();
    summary.programMaxBytes = parseInt64(m.captured(3));
    summary.rawProgramLine = m.captured(0).trimmed();
  }

  for (auto it = ramRe.globalMatch(text); it.hasNext();) {
    const QRegularExpressionMatch m = it.next();
    summary.hasRam = true;
    summary.ramUsedBytes = parseInt64(m.captured(1));
    summary.ramUsedPct = m.captured(2).toInt();
    summary.ramFreeBytes = parseInt64(m.captured(3));
    summary.ramMaxBytes = parseInt64(m.captured(4));
    summary.rawRamLine = m.captured(0).trimmed();
  }

  return summary;
}


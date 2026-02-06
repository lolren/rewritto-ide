#pragma once

#include <QString>

struct BuildSizeSummary final {
  bool hasProgram = false;
  qint64 programUsedBytes = 0;
  int programUsedPct = 0;
  qint64 programMaxBytes = 0;

  bool hasRam = false;
  qint64 ramUsedBytes = 0;
  int ramUsedPct = 0;
  qint64 ramFreeBytes = 0;
  qint64 ramMaxBytes = 0;

  QString rawProgramLine;
  QString rawRamLine;

  bool isEmpty() const { return !hasProgram && !hasRam; }

  // Compact representation intended for the status bar.
  QString toStatusText() const;
};

BuildSizeSummary parseBuildSizeSummary(const QString& output);


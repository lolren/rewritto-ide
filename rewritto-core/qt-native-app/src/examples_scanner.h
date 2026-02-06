#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct ExampleSketch final {
  QStringList menuPath;
  QString folderPath;
  QString inoPath;
};

class ExamplesScanner final {
 public:
  struct Options final {
    QString sketchbookDir;
    QString dataDir;
    QString builtinDir;
    QString currentFqbn;
  };

  static Options defaultOptions();
  static QVector<ExampleSketch> scan(const Options& options);
};


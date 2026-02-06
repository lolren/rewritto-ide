#include "examples_scanner.h"

#include <algorithm>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QVersionNumber>

namespace {
QString defaultSketchbookDir() {
  const QString home = QDir::homePath();
  const QString preferred = home + QStringLiteral("/Rewritto");
  const QString previous = home + QStringLiteral("/BlingBlink");
  const QString legacy = home + QStringLiteral("/Arduino");
  if (QDir(legacy).exists() && !QDir(preferred).exists() && !QDir(previous).exists()) {
    return legacy;
  }
  if (QDir(previous).exists() && !QDir(preferred).exists()) {
    return previous;
  }
  return preferred;
}

QString defaultDataDir() {
  return QDir::homePath() + QStringLiteral("/.arduino15");
}

QString chooseMainIno(const QString& folderPath, QStringList inos) {
  if (inos.isEmpty()) {
    return {};
  }
  std::sort(inos.begin(), inos.end());
  const QString folderName = QFileInfo(folderPath).fileName();
  for (const QString& ino : inos) {
    if (QFileInfo(ino).completeBaseName() == folderName) {
      return ino;
    }
  }
  return inos.front();
}

QString bestVersionDir(QStringList versions) {
  if (versions.isEmpty()) {
    return {};
  }

  struct Candidate final {
    QString name;
    QVersionNumber version;
    bool parsed = false;
  };

  QVector<Candidate> candidates;
  candidates.reserve(versions.size());
  for (const QString& v : versions) {
    Candidate c;
    c.name = v;
    c.version = QVersionNumber::fromString(v);
    c.parsed = !c.version.isNull();
    candidates.push_back(std::move(c));
  }

  auto better = [](const Candidate& a, const Candidate& b) {
    if (a.parsed && b.parsed) {
      return QVersionNumber::compare(a.version, b.version) > 0;
    }
    if (a.parsed != b.parsed) {
      return a.parsed;
    }
    return a.name > b.name;
  };

  Candidate best = candidates.front();
  for (const Candidate& c : candidates) {
    if (better(c, best)) {
      best = c;
    }
  }
  return best.name;
}

void collectExamplesRoot(const QString& examplesRoot,
                         const QStringList& prefix,
                         QVector<ExampleSketch>* out) {
  if (!out) {
    return;
  }
  const QFileInfo rootInfo(examplesRoot);
  if (!rootInfo.exists() || !rootInfo.isDir()) {
    return;
  }

  QHash<QString, QStringList> folderToInos;
  QDirIterator it(examplesRoot, QStringList{QStringLiteral("*.ino")}, QDir::Files,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString ino = it.next();
    const QString folder = QFileInfo(ino).absolutePath();
    folderToInos[folder].push_back(ino);
  }

  const QDir rootDir(examplesRoot);
  for (auto it2 = folderToInos.constBegin(); it2 != folderToInos.constEnd(); ++it2) {
    const QString folder = it2.key();
    const QString mainIno = chooseMainIno(folder, it2.value());
    if (mainIno.isEmpty()) {
      continue;
    }

    const QString rel = rootDir.relativeFilePath(folder);
    QStringList segments = rel.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    segments.erase(std::remove(segments.begin(), segments.end(), QStringLiteral(".")),
                   segments.end());
    if (segments.isEmpty()) {
      continue;
    }

    ExampleSketch ex;
    ex.menuPath = prefix + segments;
    ex.folderPath = folder;
    ex.inoPath = mainIno;
    out->push_back(std::move(ex));
  }
}

void collectBuiltInExamples(const QString& builtinDir, QVector<ExampleSketch>* out) {
  if (builtinDir.isEmpty()) return;
  collectExamplesRoot(builtinDir, {QStringLiteral("Built-in Examples")}, out);
}

void collectSketchbookExamples(const QString& sketchbookDir, QVector<ExampleSketch>* out) {
  const QString root = QDir(sketchbookDir).absoluteFilePath(QStringLiteral("examples"));
  collectExamplesRoot(root, {QStringLiteral("Sketchbook")}, out);
}

void collectSketchbookLibraryExamples(const QString& sketchbookDir, QVector<ExampleSketch>* out) {
  const QString libsRoot = QDir(sketchbookDir).absoluteFilePath(QStringLiteral("libraries"));
  const QDir libsDir(libsRoot);
  if (!libsDir.exists()) {
    return;
  }

  const QStringList libs =
      libsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  for (const QString& libName : libs) {
    const QString libPath = libsDir.absoluteFilePath(libName);
    const QString examplesRoot = QDir(libPath).absoluteFilePath(QStringLiteral("examples"));
    collectExamplesRoot(examplesRoot, {QStringLiteral("Libraries"), libName}, out);
  }
}

void collectCoreLibraryExamples(const QString& dataDir, QVector<ExampleSketch>* out) {
  const QString packagesRoot =
      QDir(dataDir).absoluteFilePath(QStringLiteral("packages"));
  const QDir packagesDir(packagesRoot);
  if (!packagesDir.exists()) {
    return;
  }

  const QStringList vendors =
      packagesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  for (const QString& vendor : vendors) {
    const QDir vendorDir(packagesDir.absoluteFilePath(vendor));
    const QDir hardwareDir(vendorDir.absoluteFilePath(QStringLiteral("hardware")));
    if (!hardwareDir.exists()) {
      continue;
    }

    const QStringList archs =
        hardwareDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& arch : archs) {
      const QDir archDir(hardwareDir.absoluteFilePath(arch));
      const QStringList versions =
          archDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
      const QString best = bestVersionDir(versions);
      if (best.isEmpty()) {
        continue;
      }

      const QDir versionDir(archDir.absoluteFilePath(best));
      const QDir libsDir(versionDir.absoluteFilePath(QStringLiteral("libraries")));
      if (!libsDir.exists()) {
        continue;
      }

      const QString coreId = vendor + QLatin1Char(':') + arch;
      const QStringList libs =
          libsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
      for (const QString& libName : libs) {
        const QString libPath = libsDir.absoluteFilePath(libName);
        const QString examplesRoot = QDir(libPath).absoluteFilePath(QStringLiteral("examples"));
        collectExamplesRoot(examplesRoot,
                            {QStringLiteral("Core Libraries"), coreId, libName}, out);
      }
    }
  }
}
}  // namespace

ExamplesScanner::Options ExamplesScanner::defaultOptions() {
  Options o;
  o.sketchbookDir = defaultSketchbookDir();
  o.dataDir = defaultDataDir();
  // Linux standard paths
  o.builtinDir = QStringLiteral("/usr/share/arduino/examples");
  if (!QDir(o.builtinDir).exists()) {
      o.builtinDir = QDir::homePath() + "/.arduino15/examples";
  }
  return o;
}

QVector<ExampleSketch> ExamplesScanner::scan(const Options& inOptions) {
  Options options = inOptions;
  if (options.sketchbookDir.isEmpty()) options.sketchbookDir = defaultSketchbookDir();
  if (options.dataDir.isEmpty()) options.dataDir = defaultDataDir();

  QVector<ExampleSketch> raw;
  collectBuiltInExamples(options.builtinDir, &raw);
  collectCoreLibraryExamples(options.dataDir, &raw);
  collectSketchbookLibraryExamples(options.sketchbookDir, &raw);
  collectSketchbookExamples(options.sketchbookDir, &raw);

  // Grouping and Prioritization logic
  QString activePackager;
  if (!options.currentFqbn.isEmpty()) {
      activePackager = options.currentFqbn.split(':').first();
  }

  // Sorting with Priority (IDE 2.x mechanism)
  std::sort(raw.begin(), raw.end(), [&](const ExampleSketch& a, const ExampleSketch& b) {
    auto score = [&](const ExampleSketch& s) {
        QString cat = s.menuPath.first();
        if (cat == "Built-in Examples") return 1;
        if (cat == "Core Libraries") {
            if (!activePackager.isEmpty() && s.menuPath.size() > 1) {
                const QString coreId = s.menuPath.at(1);
                if (coreId.startsWith(activePackager + ":")) return 2;
            }
            return 5; // Non-selected cores go way down
        }
        if (cat == "Libraries") return 3;
        if (cat == "Sketchbook") return 4;
        return 6;
    };

    int scoreA = score(a);
    int scoreB = score(b);
    if (scoreA != scoreB) return scoreA < scoreB;

    const QString ap = a.menuPath.join(QLatin1Char('/'));
    const QString bp = b.menuPath.join(QLatin1Char('/'));
    if (ap != bp) return ap < bp;
    return a.folderPath < b.folderPath;
  });

  return raw;
}

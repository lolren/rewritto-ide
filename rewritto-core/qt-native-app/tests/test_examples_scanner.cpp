#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "examples_scanner.h"

class TestExamplesScanner final : public QObject {
  Q_OBJECT

 private slots:
  void findsSketchbookAndCoreExamples();
};

static bool writeTextFile(const QString& path, const QByteArray& data = {}) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    return false;
  }
  if (!data.isEmpty()) {
    f.write(data);
  }
  return true;
}

void TestExamplesScanner::findsSketchbookAndCoreExamples() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const QString sketchbook = dir.filePath("Rewritto");
  const QString dataDir = dir.filePath(".arduino15");

  // Sketchbook examples
  QVERIFY(writeTextFile(dir.filePath("Rewritto/examples/Basic/Blink/Blink.ino"),
                        "void setup() {}\nvoid loop() {}\n"));

  // Sketchbook library examples (include extra .ino to validate "main" selection)
  QVERIFY(writeTextFile(dir.filePath("Rewritto/libraries/Foo/examples/Bar/Bar.ino"),
                        "void setup() {}\nvoid loop() {}\n"));
  QVERIFY(writeTextFile(dir.filePath("Rewritto/libraries/Foo/examples/Bar/Helper.ino"),
                        "// helper\n"));
  QVERIFY(writeTextFile(dir.filePath("Rewritto/libraries/Foo/examples/Communication/Baz/Baz.ino"),
                        "void setup() {}\nvoid loop() {}\n"));

  // Core examples: ensure only best (highest) version is scanned.
  QVERIFY(writeTextFile(dir.filePath(
      ".arduino15/packages/vendor/hardware/arch/1.0.0/libraries/CoreLib/examples/Old/Old.ino"),
                        "void setup() {}\nvoid loop() {}\n"));
  QVERIFY(writeTextFile(dir.filePath(
      ".arduino15/packages/vendor/hardware/arch/2.0.0/libraries/CoreLib/examples/New/New.ino"),
                        "void setup() {}\nvoid loop() {}\n"));

  ExamplesScanner::Options options;
  options.sketchbookDir = sketchbook;
  options.dataDir = dataDir;

  const QVector<ExampleSketch> examples = ExamplesScanner::scan(options);
  QVERIFY(!examples.isEmpty());

  auto findByPath = [&examples](const QStringList& menuPath) -> const ExampleSketch* {
    for (const auto& ex : examples) {
      if (ex.menuPath == menuPath) {
        return &ex;
      }
    }
    return nullptr;
  };

  const ExampleSketch* blink = findByPath({"Sketchbook", "Basic", "Blink"});
  QVERIFY(blink);
  QVERIFY(QFileInfo(blink->inoPath).fileName() == "Blink.ino");

  const ExampleSketch* bar = findByPath({"Libraries", "Foo", "Bar"});
  QVERIFY(bar);
  QVERIFY(QFileInfo(bar->inoPath).fileName() == "Bar.ino");

  const ExampleSketch* baz = findByPath({"Libraries", "Foo", "Communication", "Baz"});
  QVERIFY(baz);

  const ExampleSketch* coreNew =
      findByPath({"Core Libraries", "vendor:arch", "CoreLib", "New"});
  QVERIFY(coreNew);
  QVERIFY(coreNew->inoPath.contains("/2.0.0/"));

  const ExampleSketch* coreOld =
      findByPath({"Core Libraries", "vendor:arch", "CoreLib", "Old"});
  QVERIFY(!coreOld);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TestExamplesScanner tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_examples_scanner.moc"

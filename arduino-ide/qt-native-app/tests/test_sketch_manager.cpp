#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "sketch_manager.h"

class TestSketchManager final : public QObject {
  Q_OBJECT

 private slots:
  void cloneSketchRenamesPrimaryInoAndCopiesFiles();
  void cloneSketchRenamesFirstInoWhenPrimaryMissing();
};

static bool writeTextFile(const QString& path, const QByteArray& bytes) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    return false;
  }
  if (f.write(bytes) != bytes.size()) {
    return false;
  }
  return true;
}

void TestSketchManager::cloneSketchRenamesPrimaryInoAndCopiesFiles() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());

  const QString srcParent = QDir(tmp.path()).absoluteFilePath("src");
  const QString dstParent = QDir(tmp.path()).absoluteFilePath("dst");
  QVERIFY(QDir().mkpath(srcParent));
  QVERIFY(QDir().mkpath(dstParent));

  const QString srcSketch = QDir(srcParent).absoluteFilePath("OldSketch");
  QVERIFY(QDir().mkpath(srcSketch));
  QVERIFY(writeTextFile(QDir(srcSketch).absoluteFilePath("OldSketch.ino"),
                        "void setup(){}\n"));
  QVERIFY(writeTextFile(QDir(srcSketch).absoluteFilePath("util.h"),
                        "#pragma once\n"));
  QVERIFY(writeTextFile(QDir(srcSketch).absoluteFilePath("data/payload.txt"),
                        "hello\n"));

  QString outFolder;
  QString err;
  QVERIFY(SketchManager::cloneSketchFolder(srcSketch, dstParent, "NewSketch", &outFolder, &err));
  QVERIFY(err.isEmpty());
  QCOMPARE(QFileInfo(outFolder).fileName(), QStringLiteral("NewSketch"));

  const QString dstSketch = QDir(dstParent).absoluteFilePath("NewSketch");
  QVERIFY(QFileInfo(dstSketch).isDir());

  QVERIFY(QFileInfo(QDir(dstSketch).absoluteFilePath("NewSketch.ino")).isFile());
  QVERIFY(!QFileInfo(QDir(dstSketch).absoluteFilePath("OldSketch.ino")).exists());
  QVERIFY(QFileInfo(QDir(dstSketch).absoluteFilePath("util.h")).isFile());
  QVERIFY(QFileInfo(QDir(dstSketch).absoluteFilePath("data/payload.txt")).isFile());

  QFile ino(QDir(dstSketch).absoluteFilePath("NewSketch.ino"));
  QVERIFY(ino.open(QIODevice::ReadOnly | QIODevice::Text));
  const QByteArray inoBytes = ino.readAll();
  QVERIFY(inoBytes.contains("void setup"));
}

void TestSketchManager::cloneSketchRenamesFirstInoWhenPrimaryMissing() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());

  const QString srcParent = QDir(tmp.path()).absoluteFilePath("src");
  const QString dstParent = QDir(tmp.path()).absoluteFilePath("dst");
  QVERIFY(QDir().mkpath(srcParent));
  QVERIFY(QDir().mkpath(dstParent));

  const QString srcSketch = QDir(srcParent).absoluteFilePath("Weird");
  QVERIFY(QDir().mkpath(srcSketch));
  QVERIFY(writeTextFile(QDir(srcSketch).absoluteFilePath("foo.ino"),
                        "int x = 1;\n"));

  QString outFolder;
  QString err;
  QVERIFY(SketchManager::cloneSketchFolder(srcSketch, dstParent, "Normalized", &outFolder, &err));
  QVERIFY(err.isEmpty());

  const QString dstSketch = QDir(dstParent).absoluteFilePath("Normalized");
  QVERIFY(QFileInfo(dstSketch).isDir());
  QVERIFY(QFileInfo(QDir(dstSketch).absoluteFilePath("Normalized.ino")).isFile());
  QVERIFY(!QFileInfo(QDir(dstSketch).absoluteFilePath("foo.ino")).exists());
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TestSketchManager tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_sketch_manager.moc"

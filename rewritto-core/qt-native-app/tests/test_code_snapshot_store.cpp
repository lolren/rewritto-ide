#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "code_snapshot_store.h"

class TestCodeSnapshotStore final : public QObject {
  Q_OBJECT

 private slots:
  void createsListsRestoresAndDeletes();
};

void TestCodeSnapshotStore::createsListsRestoresAndDeletes() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const QString sketch = QDir(dir.path()).absoluteFilePath("sketch");
  QVERIFY(QDir().mkpath(sketch));

  QFile ino(QDir(sketch).filePath("sketch.ino"));
  QVERIFY(ino.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QVERIFY(ino.write("void setup() {}\nvoid loop() {}\n") > 0);
  ino.close();

  QVERIFY(QDir().mkpath(QDir(sketch).filePath("sub")));
  QFile header(QDir(sketch).filePath("sub/a.h"));
  QVERIFY(header.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QVERIFY(header.write("#pragma once\n") > 0);
  header.close();

  // Ensure ignored folders are not included.
  QVERIFY(QDir().mkpath(QDir(sketch).filePath(".rewritto/snapshots")));
  QFile ignoredRewritto(QDir(sketch).filePath(".rewritto/snapshots/ignored.txt"));
  QVERIFY(ignoredRewritto.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QVERIFY(ignoredRewritto.write("ignore\n") > 0);
  ignoredRewritto.close();

  QVERIFY(QDir().mkpath(QDir(sketch).filePath(".git")));
  QFile ignoredGit(QDir(sketch).filePath(".git/ignored"));
  QVERIFY(ignoredGit.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QVERIFY(ignoredGit.write("ignore\n") > 0);
  ignoredGit.close();

  CodeSnapshotStore::CreateOptions options;
  options.sketchFolder = sketch;
  options.comment = QStringLiteral("first");
  options.fileOverrides.insert(QStringLiteral("sketch.ino"), QByteArray("unsaved\n"));

  QString err;
  CodeSnapshotStore::SnapshotMeta meta;
  QVERIFY2(CodeSnapshotStore::createSnapshot(options, &meta, &err),
           qPrintable(err));
  QVERIFY(!meta.id.isEmpty());
  QCOMPARE(meta.comment, QStringLiteral("first"));
  QVERIFY(meta.fileCount >= 2);

  const QVector<CodeSnapshotStore::SnapshotMeta> list =
      CodeSnapshotStore::listSnapshots(sketch, &err);
  QCOMPARE(list.size(), 1);
  QCOMPARE(list.at(0).id, meta.id);

  const auto snapshot = CodeSnapshotStore::readSnapshot(sketch, meta.id, &err);
  QVERIFY2(snapshot.has_value(), qPrintable(err));

  bool sawSketch = false;
  bool sawIgnored = false;
  QString sketchSha1;
  for (const auto& f : snapshot->files) {
    if (f.relativePath == QStringLiteral("sketch.ino")) {
      sawSketch = true;
      sketchSha1 = f.sha1Hex;
    }
    if (f.relativePath.startsWith(QStringLiteral(".rewritto/")) ||
        f.relativePath.startsWith(QStringLiteral(".git/"))) {
      sawIgnored = true;
    }
  }
  QVERIFY(sawSketch);
  QVERIFY(!sawIgnored);

  const QString expectedSha1 =
      QString::fromLatin1(QCryptographicHash::hash(QByteArray("unsaved\n"),
                                                  QCryptographicHash::Sha1)
                              .toHex());
  QCOMPARE(sketchSha1.toLower(), expectedSha1);

  QFile changed(QDir(sketch).filePath("sketch.ino"));
  QVERIFY(changed.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QVERIFY(changed.write("changed\n") > 0);
  changed.close();

  QStringList written;
  QVERIFY2(CodeSnapshotStore::restoreSnapshot(sketch, meta.id, &written, &err),
           qPrintable(err));
  QVERIFY(written.contains(QFileInfo(QDir(sketch).filePath("sketch.ino")).absoluteFilePath()));

  QFile restored(QDir(sketch).filePath("sketch.ino"));
  QVERIFY(restored.open(QIODevice::ReadOnly));
  QCOMPARE(restored.readAll(), QByteArray("unsaved\n"));
  restored.close();

  QVERIFY2(CodeSnapshotStore::updateSnapshotComment(sketch, meta.id, QStringLiteral("updated"), &err),
           qPrintable(err));
  const auto snapshot2 = CodeSnapshotStore::readSnapshot(sketch, meta.id, &err);
  QVERIFY2(snapshot2.has_value(), qPrintable(err));
  QCOMPARE(snapshot2->meta.comment, QStringLiteral("updated"));

  QVERIFY2(CodeSnapshotStore::deleteSnapshot(sketch, meta.id, &err),
           qPrintable(err));
  const QVector<CodeSnapshotStore::SnapshotMeta> list2 =
      CodeSnapshotStore::listSnapshots(sketch, &err);
  QCOMPARE(list2.size(), 0);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TestCodeSnapshotStore tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_code_snapshot_store.moc"


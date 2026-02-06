#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include "replace_in_files_dialog.h"

class TestReplaceInFilesWorker final : public QObject {
  Q_OBJECT

 private slots:
  void replacesAcrossFiles_preservesCrlf();
  void replacesWholeWordsOnly();
};

static void writeFile(const QString& path, const QByteArray& data) {
  QFile f(path);
  QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QVERIFY(f.write(data) == data.size());
}

static QByteArray readFile(const QString& path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    return {};
  }
  return f.readAll();
}

void TestReplaceInFilesWorker::replacesAcrossFiles_preservesCrlf() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const QString a = dir.filePath("a.ino");
  const QString b = dir.filePath("b.cpp");
  writeFile(a, "hello\r\nHello\r\n");
  writeFile(b, "nope\nheLLo there\n");

  ReplaceInFilesWorker worker;
  QSignalSpy finishedSpy(&worker, &ReplaceInFilesWorker::applyFinished);

  worker.apply(dir.path(), "hello", "bye", {"*.ino", "*.cpp"}, false, false);

  QCOMPARE(finishedSpy.count(), 1);
  const auto args = finishedSpy.takeFirst();
  const int matchesReplaced = args.at(0).toInt();
  const int filesScanned = args.at(1).toInt();
  const QStringList modified = args.at(2).toStringList();

  QVERIFY(matchesReplaced >= 3);
  QCOMPARE(filesScanned, 2);
  QVERIFY(modified.contains(a));
  QVERIFY(modified.contains(b));

  const QByteArray aOut = readFile(a);
  QVERIFY(aOut.contains("bye\r\nBye\r\n") || aOut.contains("bye\r\nbye\r\n"));
  // Ensure CRLF is preserved on every newline.
  for (int i = 0; i < aOut.size(); ++i) {
    if (aOut.at(i) == '\n') {
      QVERIFY(i > 0);
      QCOMPARE(aOut.at(i - 1), '\r');
    }
  }

  const QByteArray bOut = readFile(b);
  QVERIFY(bOut.contains("bye there"));
}

void TestReplaceInFilesWorker::replacesWholeWordsOnly() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const QString a = dir.filePath("a.ino");
  writeFile(a, "int x = 0;\ninteger y = 1;\nintz z = 2;\n");

  ReplaceInFilesWorker worker;
  QSignalSpy finishedSpy(&worker, &ReplaceInFilesWorker::applyFinished);

  worker.apply(dir.path(), "int", "short", {"*.ino"}, true, true);

  QCOMPARE(finishedSpy.count(), 1);
  const QByteArray out = readFile(a);
  QVERIFY(out.contains("short x = 0;"));
  QVERIFY(out.contains("integer y = 1;"));
  QVERIFY(out.contains("intz z = 2;"));
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TestReplaceInFilesWorker tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_replace_in_files_worker.moc"


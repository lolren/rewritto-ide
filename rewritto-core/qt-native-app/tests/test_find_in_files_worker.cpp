#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include "find_in_files_dialog.h"

class TestFindInFilesWorker final : public QObject {
  Q_OBJECT

 private slots:
  void findsMatches_caseInsensitive();
  void findsMatches_caseSensitive();
  void respectsExcludePatterns();
};

static void writeFile(const QString& path, const QByteArray& data) {
  QFile f(path);
  QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
  QVERIFY(f.write(data) == data.size());
}

void TestFindInFilesWorker::findsMatches_caseInsensitive() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  writeFile(dir.filePath("a.ino"), "hello\nHello\n");
  writeFile(dir.filePath("b.cpp"), "nope\nheLLo there\n");

  FindInFilesWorker worker;
  QSignalSpy matchSpy(&worker, &FindInFilesWorker::matchFound);
  QSignalSpy finishedSpy(&worker, &FindInFilesWorker::finished);

  worker.run(dir.path(), "hello", {"*.ino", "*.cpp"}, {}, false);

  QCOMPARE(finishedSpy.count(), 1);
  QVERIFY(matchSpy.count() >= 3);
}

void TestFindInFilesWorker::findsMatches_caseSensitive() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  writeFile(dir.filePath("a.ino"), "hello\nHello\n");
  writeFile(dir.filePath("b.cpp"), "hello\n");

  FindInFilesWorker worker;
  QSignalSpy matchSpy(&worker, &FindInFilesWorker::matchFound);
  QSignalSpy finishedSpy(&worker, &FindInFilesWorker::finished);

  worker.run(dir.path(), "Hello", {"*.ino", "*.cpp"}, {}, true);

  QCOMPARE(finishedSpy.count(), 1);
  QCOMPARE(matchSpy.count(), 1);

  const auto args = matchSpy.takeFirst();
  const QString filePath = args.at(0).toString();
  const int line = args.at(1).toInt();
  const int col = args.at(2).toInt();
  const QString preview = args.at(3).toString();

  QVERIFY(filePath.endsWith("a.ino"));
  QCOMPARE(line, 2);
  QCOMPARE(col, 1);
  QCOMPARE(preview, QStringLiteral("Hello"));
}

void TestFindInFilesWorker::respectsExcludePatterns() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  writeFile(dir.filePath("a.ino"), "hello\n");
  writeFile(dir.filePath("b.cpp"), "hello\n");

  FindInFilesWorker worker;
  QSignalSpy matchSpy(&worker, &FindInFilesWorker::matchFound);
  QSignalSpy finishedSpy(&worker, &FindInFilesWorker::finished);

  worker.run(dir.path(), "hello", {"*.ino", "*.cpp"}, {"b.cpp"}, false);

  QCOMPARE(finishedSpy.count(), 1);
  QVERIFY(matchSpy.count() >= 1);
  for (const auto& args : matchSpy) {
    const QString filePath = args.at(0).toString();
    QVERIFY(!filePath.endsWith("b.cpp"));
  }
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TestFindInFilesWorker tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_find_in_files_worker.moc"

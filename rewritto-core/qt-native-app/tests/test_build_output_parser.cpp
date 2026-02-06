#include <QtTest/QtTest>

#include <QCoreApplication>

#include "build_output_parser.h"

class TestBuildOutputParser final : public QObject {
  Q_OBJECT

 private slots:
  void parsesSizeSummary();
  void usesLastMatch();
  void handlesCommaSeparatedNumbers();
  void returnsEmptyWhenNoMatches();
};

void TestBuildOutputParser::parsesSizeSummary() {
  const QString out =
      "Sketch uses 924 bytes (2%) of program storage space. Maximum is 32256 bytes.\n"
      "Global variables use 9 bytes (0%) of dynamic memory, leaving 2039 bytes for local variables. Maximum is 2048 bytes.\n";

  const BuildSizeSummary s = parseBuildSizeSummary(out);
  QVERIFY(!s.isEmpty());

  QVERIFY(s.hasProgram);
  QCOMPARE(s.programUsedBytes, 924);
  QCOMPARE(s.programUsedPct, 2);
  QCOMPARE(s.programMaxBytes, 32256);

  QVERIFY(s.hasRam);
  QCOMPARE(s.ramUsedBytes, 9);
  QCOMPARE(s.ramUsedPct, 0);
  QCOMPARE(s.ramFreeBytes, 2039);
  QCOMPARE(s.ramMaxBytes, 2048);

  QCOMPARE(s.toStatusText(), QStringLiteral("Flash 924 B (2%) | RAM 9 B (0%)"));
}

void TestBuildOutputParser::usesLastMatch() {
  const QString out =
      "Sketch uses 1 bytes (0%) of program storage space. Maximum is 10 bytes.\n"
      "Global variables use 1 bytes (0%) of dynamic memory, leaving 2 bytes for local variables. Maximum is 3 bytes.\n"
      "...\n"
      "Sketch uses 222 bytes (3%) of program storage space. Maximum is 4444 bytes.\n"
      "Global variables use 55 bytes (6%) of dynamic memory, leaving 777 bytes for local variables. Maximum is 8888 bytes.\n";

  const BuildSizeSummary s = parseBuildSizeSummary(out);
  QVERIFY(s.hasProgram);
  QCOMPARE(s.programUsedBytes, 222);
  QCOMPARE(s.programUsedPct, 3);
  QCOMPARE(s.programMaxBytes, 4444);

  QVERIFY(s.hasRam);
  QCOMPARE(s.ramUsedBytes, 55);
  QCOMPARE(s.ramUsedPct, 6);
  QCOMPARE(s.ramFreeBytes, 777);
  QCOMPARE(s.ramMaxBytes, 8888);
}

void TestBuildOutputParser::handlesCommaSeparatedNumbers() {
  const QString out =
      "Sketch uses 12,345 bytes (10%) of program storage space. Maximum is 98,765 bytes.\n"
      "Global variables use 1,234 bytes (5%) of dynamic memory, leaving 56,789 bytes for local variables. Maximum is 67,890 bytes.\n";

  const BuildSizeSummary s = parseBuildSizeSummary(out);
  QVERIFY(s.hasProgram);
  QCOMPARE(s.programUsedBytes, 12345);
  QCOMPARE(s.programUsedPct, 10);
  QCOMPARE(s.programMaxBytes, 98765);

  QVERIFY(s.hasRam);
  QCOMPARE(s.ramUsedBytes, 1234);
  QCOMPARE(s.ramUsedPct, 5);
  QCOMPARE(s.ramFreeBytes, 56789);
  QCOMPARE(s.ramMaxBytes, 67890);
}

void TestBuildOutputParser::returnsEmptyWhenNoMatches() {
  const BuildSizeSummary s = parseBuildSizeSummary(QStringLiteral("no size here\n"));
  QVERIFY(s.isEmpty());
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setOrganizationName("RewrittoIdeTests");
  QCoreApplication::setApplicationName("test_build_output_parser");
  TestBuildOutputParser tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_build_output_parser.moc"


#include <QtTest/QtTest>

#include <QCoreApplication>

#include "serial_plot_parser.h"

class TestSerialPlotParser final : public QObject {
  Q_OBJECT

 private slots:
  void parsesNumbersFromLine();
  void parsesLabelsWhenPresent();
};

void TestSerialPlotParser::parsesNumbersFromLine() {
  SerialPlotParser p;

  const QVector<double> v1 = p.parseLine("1 2 3");
  QCOMPARE(v1.size(), 3);
  QCOMPARE(v1[0], 1.0);
  QCOMPARE(v1[1], 2.0);
  QCOMPARE(v1[2], 3.0);

  const QVector<double> v2 = p.parseLine("temp=21.5, hum=0.45");
  QCOMPARE(v2.size(), 2);
  QCOMPARE(v2[0], 21.5);
  QCOMPARE(v2[1], 0.45);

  const QVector<double> v3 = p.parseLine("-3.5e2 foo +1.2E-1");
  QCOMPARE(v3.size(), 2);
  QCOMPARE(v3[0], -350.0);
  QCOMPARE(v3[1], 0.12);
}

void TestSerialPlotParser::parsesLabelsWhenPresent() {
  SerialPlotParser p;

  const SerialPlotSample s1 = p.parseSample("temp=21.5, hum=0.45");
  QCOMPARE(s1.labels.size(), 2);
  QCOMPARE(s1.values.size(), 2);
  QCOMPARE(s1.labels[0], QStringLiteral("temp"));
  QCOMPARE(s1.labels[1], QStringLiteral("hum"));
  QCOMPARE(s1.values[0], 21.5);
  QCOMPARE(s1.values[1], 0.45);

  const SerialPlotSample s2 = p.parseSample("1 2 3");
  QCOMPARE(s2.labels.size(), 0);
  QCOMPARE(s2.values.size(), 3);
  QCOMPARE(s2.values[0], 1.0);
  QCOMPARE(s2.values[1], 2.0);
  QCOMPARE(s2.values[2], 3.0);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TestSerialPlotParser tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_serial_plot_parser.moc"

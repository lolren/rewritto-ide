#include <QtTest/QtTest>

#include <cmath>

#include "serial_plot_range.h"

class TestSerialPlotRange final : public QObject {
  Q_OBJECT

 private slots:
  void computesAutoRangeIgnoresNaN();
  void freezeHoldsRange();
  void manualRangeOverridesAutoscale();
};

void TestSerialPlotRange::computesAutoRangeIgnoresNaN() {
  QVector<QVector<double>> series;
  series.push_back({1.0, 2.0, std::nan("")});
  series.push_back({-3.0, std::nan(""), 5.0});

  const SerialPlotYRange r = serialPlotComputeAutoRange(series);
  QVERIFY(r.hasValue);
  QCOMPARE(r.minY, -3.0);
  QCOMPARE(r.maxY, 5.0);
}

void TestSerialPlotRange::freezeHoldsRange() {
  SerialPlotRangeController c;
  c.updateAutoRange(SerialPlotYRange{true, 0.0, 10.0});
  c.setFreezeEnabled(true);
  {
    const SerialPlotYRange r = c.currentRange();
    QVERIFY(r.hasValue);
    QCOMPARE(r.minY, 0.0);
    QCOMPARE(r.maxY, 10.0);
  }

  c.updateAutoRange(SerialPlotYRange{true, -5.0, 25.0});
  {
    const SerialPlotYRange r = c.currentRange();
    QVERIFY(r.hasValue);
    QCOMPARE(r.minY, 0.0);
    QCOMPARE(r.maxY, 10.0);
  }

  c.setFreezeEnabled(false);
  {
    const SerialPlotYRange r = c.currentRange();
    QVERIFY(r.hasValue);
    QCOMPARE(r.minY, -5.0);
    QCOMPARE(r.maxY, 25.0);
  }
}

void TestSerialPlotRange::manualRangeOverridesAutoscale() {
  SerialPlotRangeController c;
  c.updateAutoRange(SerialPlotYRange{true, -100.0, 100.0});
  c.setManualRange(-2.0, 2.0);
  c.setAutoScaleEnabled(false);

  const SerialPlotYRange r = c.currentRange();
  QVERIFY(r.hasValue);
  QCOMPARE(r.minY, -2.0);
  QCOMPARE(r.maxY, 2.0);
}

QTEST_MAIN(TestSerialPlotRange)

#include "test_serial_plot_range.moc"


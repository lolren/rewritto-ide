#include <QtTest/QtTest>

#include <QApplication>
#include <QCheckBox>
#include <QTemporaryDir>
#include <QTreeWidget>

#include "serial_plotter_widget.h"

class TestSerialPlotterWidget final : public QObject {
  Q_OBJECT

 private slots:
  void autoReconnectPersists();
  void legendPopulatesFromData();
};

void TestSerialPlotterWidget::autoReconnectPersists() {
  {
    SerialPlotterWidget w;
    w.resize(700, 200);
    w.show();

    auto* cb = w.findChild<QCheckBox*>("serialPlotterAutoReconnect");
    QVERIFY(cb);
    cb->setChecked(true);
    QVERIFY(cb->isChecked());
  }

  {
    SerialPlotterWidget w;
    w.resize(700, 200);
    w.show();

    auto* cb = w.findChild<QCheckBox*>("serialPlotterAutoReconnect");
    QVERIFY(cb);
    QVERIFY(cb->isChecked());
  }
}

void TestSerialPlotterWidget::legendPopulatesFromData() {
  SerialPlotterWidget w;
  w.resize(700, 260);
  w.show();

  auto* legend = w.findChild<QTreeWidget*>("serialPlotterLegend");
  QVERIFY(legend);
  QCOMPARE(legend->topLevelItemCount(), 0);

  w.appendData(QByteArray("a=1,b=2\n"));
  QTRY_COMPARE(legend->topLevelItemCount(), 2);
  QCOMPARE(legend->topLevelItem(0)->text(0), QString("a"));
  QCOMPARE(legend->topLevelItem(1)->text(0), QString("b"));

  legend->topLevelItem(0)->setCheckState(0, Qt::Unchecked);
  QCOMPARE(legend->topLevelItem(0)->checkState(0), Qt::Unchecked);
}

int main(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", "offscreen");

  QTemporaryDir tmp;
  if (!tmp.isValid()) {
    return 1;
  }
  qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());
  qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
  qputenv("XDG_CACHE_HOME", tmp.path().toUtf8());

  QApplication app(argc, argv);
  QApplication::setOrganizationName("BlingBlinkTest");
  QApplication::setApplicationName("SerialPlotterTest");

  TestSerialPlotterWidget tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_serial_plotter_widget.moc"

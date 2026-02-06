#include <QtTest/QtTest>

#include <QApplication>
#include <QLineEdit>
#include <QTableView>
#include <QTemporaryDir>

#include "quick_pick_dialog.h"

class TestQuickPickDialog final : public QObject {
  Q_OBJECT

 private slots:
  void fuzzyFiltersBySubsequence();
};

void TestQuickPickDialog::fuzzyFiltersBySubsequence() {
  QuickPickDialog dlg;
  dlg.resize(640, 480);
  dlg.show();

  QVector<QuickPickDialog::Item> items;
  items.push_back({"a_b_c", "detail", 1});
  items.push_back({"xyz", "detail", 2});
  dlg.setItems(items);

  auto* edit = dlg.findChild<QLineEdit*>();
  QVERIFY(edit);
  auto* table = dlg.findChild<QTableView*>();
  QVERIFY(table);

  QCOMPARE(table->model()->rowCount(), 2);

  edit->setText("abc");
  QCoreApplication::processEvents();
  QCOMPARE(table->model()->rowCount(), 1);

  edit->setText("zz");
  QCoreApplication::processEvents();
  QCOMPARE(table->model()->rowCount(), 0);

  edit->clear();
  QCoreApplication::processEvents();
  QCOMPARE(table->model()->rowCount(), 2);
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
  QApplication::setApplicationName("QuickPickTest");

  TestQuickPickDialog tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_quick_pick_dialog.moc"

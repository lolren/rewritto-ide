#include <QtTest/QtTest>

#include <QApplication>
#include <QFileInfo>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "welcome_widget.h"

class TestWelcomeWidget final : public QObject {
  Q_OBJECT

 private slots:
  void showsPinnedAndRecentLists();
  void emitsOpenSketchSelectedOnActivation();
};

void TestWelcomeWidget::showsPinnedAndRecentLists() {
  QTemporaryDir d1;
  QTemporaryDir d2;
  QTemporaryDir d3;
  QVERIFY(d1.isValid());
  QVERIFY(d2.isValid());
  QVERIFY(d3.isValid());

  WelcomeWidget w;
  w.resize(900, 600);
  w.show();

  auto* pinned = w.findChild<QListWidget*>("WelcomePinnedList");
  QVERIFY(pinned);
  auto* recent = w.findChild<QListWidget*>("WelcomeRecentList");
  QVERIFY(recent);
  auto* clearPinned = w.findChild<QPushButton*>("WelcomeClearPinnedButton");
  QVERIFY(clearPinned);
  auto* clearRecent = w.findChild<QPushButton*>("WelcomeClearRecentButton");
  QVERIFY(clearRecent);

  w.setPinnedSketches({d1.path(), d2.path()});
  w.setRecentSketches({d3.path()});
  QCoreApplication::processEvents();

  QCOMPARE(pinned->count(), 2);
  QCOMPARE(recent->count(), 1);
  QVERIFY(clearPinned->isEnabled());
  QVERIFY(clearRecent->isEnabled());
}

void TestWelcomeWidget::emitsOpenSketchSelectedOnActivation() {
  QTemporaryDir d1;
  QVERIFY(d1.isValid());

  WelcomeWidget w;
  w.resize(900, 600);
  w.show();
  w.setPinnedSketches({d1.path()});
  w.setRecentSketches({});
  QCoreApplication::processEvents();

  auto* pinned = w.findChild<QListWidget*>("WelcomePinnedList");
  QVERIFY(pinned);
  QCOMPARE(pinned->count(), 1);

  QSignalSpy spy(&w, &WelcomeWidget::openSketchSelected);
  QVERIFY(spy.isValid());

  pinned->setCurrentRow(0);
  pinned->setFocus();
  QTest::keyClick(pinned, Qt::Key_Return);
  QCoreApplication::processEvents();

  QCOMPARE(spy.count(), 1);
  const QList<QVariant> args = spy.takeFirst();
  QCOMPARE(args.size(), 1);
  QCOMPARE(QFileInfo(args.at(0).toString()).absoluteFilePath(),
           QFileInfo(d1.path()).absoluteFilePath());
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
  QApplication::setOrganizationName("RewrittoTest");
  QApplication::setApplicationName("WelcomeWidgetTest");

  TestWelcomeWidget tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_welcome_widget.moc"

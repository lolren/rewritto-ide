#include <QtTest/QtTest>

#include <QApplication>
#include <QPlainTextEdit>

#include "output_widget.h"

class TestOutputWidget final : public QObject {
  Q_OBJECT

 private slots:
  void limitsOutputBlockCount();
};

void TestOutputWidget::limitsOutputBlockCount() {
  OutputWidget w;
  auto* edit = w.findChild<QPlainTextEdit*>(QStringLiteral("OutputTextEdit"));
  QVERIFY(edit);

  const int maxBlocks = edit->document()->maximumBlockCount();
  QVERIFY2(maxBlocks > 0, "OutputTextEdit should have a maximum block count set.");

  for (int i = 0; i < maxBlocks + 200; ++i) {
    w.appendLine(QString::number(i));
  }
  QApplication::processEvents();

  // QTextDocument keeps an extra empty block at the end sometimes; allow +1.
  QVERIFY(edit->document()->blockCount() <= maxBlocks + 1);
}

int main(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  TestOutputWidget tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_output_widget.moc"


#include <QtTest/QtTest>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTemporaryDir>

#include "serial_monitor_widget.h"

class TestSerialMonitorWidget final : public QObject {
  Q_OBJECT

 private slots:
  void timestampsPrefixesLines();
  void carriageReturnOverwritesLine();
  void autoscrollToggle();
  void autoReconnectPersists();
  void emptySendEmitsLineEnding();
  void sendHistoryUpDownAndPersists();
};

void TestSerialMonitorWidget::timestampsPrefixesLines() {
  SerialMonitorWidget w;
  w.resize(700, 320);
  w.show();

  auto* output = w.findChild<QPlainTextEdit*>("serialMonitorOutput");
  QVERIFY(output);
  auto* timestamps = w.findChild<QCheckBox*>("serialMonitorTimestamps");
  QVERIFY(timestamps);

  timestamps->setChecked(true);
  w.appendData(QByteArray("hello\nworld\n"));

  const QString text = output->toPlainText();
  const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
  QCOMPARE(lines.size(), 2);

  const QRegularExpression re(
      R"(^\[\d{2}:\d{2}:\d{2}\.\d{3}\] (hello|world)$)");
  for (const QString& line : lines) {
    QVERIFY(re.match(line).hasMatch());
  }
}

void TestSerialMonitorWidget::carriageReturnOverwritesLine() {
  {
    SerialMonitorWidget w;
    w.resize(700, 320);
    w.show();

    auto* output = w.findChild<QPlainTextEdit*>("serialMonitorOutput");
    QVERIFY(output);
    auto* timestamps = w.findChild<QCheckBox*>("serialMonitorTimestamps");
    QVERIFY(timestamps);
    timestamps->setChecked(false);

    w.appendData(QByteArray("hello\rabc\n"));
    QCOMPARE(output->toPlainText(), QString("abclo\n"));
  }

  {
    SerialMonitorWidget w;
    w.resize(700, 320);
    w.show();

    auto* output = w.findChild<QPlainTextEdit*>("serialMonitorOutput");
    QVERIFY(output);
    auto* timestamps = w.findChild<QCheckBox*>("serialMonitorTimestamps");
    QVERIFY(timestamps);
    timestamps->setChecked(false);

    w.appendData(QByteArray("hello\r\nworld\n"));
    QCOMPARE(output->toPlainText(), QString("hello\nworld\n"));
  }
}

void TestSerialMonitorWidget::autoscrollToggle() {
  SerialMonitorWidget w;
  w.resize(700, 200);
  w.show();

  auto* output = w.findChild<QPlainTextEdit*>("serialMonitorOutput");
  QVERIFY(output);
  auto* autoscroll = w.findChild<QCheckBox*>("serialMonitorAutoscroll");
  QVERIFY(autoscroll);

  QByteArray manyLines;
  for (int i = 0; i < 250; ++i) {
    manyLines += "line\n";
  }

  autoscroll->setChecked(false);
  w.appendData(manyLines);

  auto* sb = output->verticalScrollBar();
  QVERIFY(sb);
  QVERIFY(sb->maximum() > 0);
  QCOMPARE(sb->value(), 0);

  autoscroll->setChecked(true);
  w.appendData(manyLines);
  QCOMPARE(sb->value(), sb->maximum());
}

void TestSerialMonitorWidget::autoReconnectPersists() {
  {
    SerialMonitorWidget w;
    w.resize(700, 200);
    w.show();

    auto* cb = w.findChild<QCheckBox*>("serialMonitorAutoReconnect");
    QVERIFY(cb);
    cb->setChecked(true);
    QVERIFY(cb->isChecked());
  }

  {
    SerialMonitorWidget w;
    w.resize(700, 200);
    w.show();

    auto* cb = w.findChild<QCheckBox*>("serialMonitorAutoReconnect");
    QVERIFY(cb);
    QVERIFY(cb->isChecked());
  }
}

void TestSerialMonitorWidget::emptySendEmitsLineEnding() {
  SerialMonitorWidget w;
  w.resize(700, 320);
  w.show();
  w.setConnected(true);

  auto* input = w.findChild<QLineEdit*>("serialMonitorInput");
  QVERIFY(input);
  auto* sendButton = w.findChild<QPushButton*>("serialMonitorSendButton");
  QVERIFY(sendButton);
  auto* lineEnding = w.findChild<QComboBox*>("serialMonitorLineEnding");
  QVERIFY(lineEnding);

  const int lfIdx = lineEnding->findData(QByteArray("\n"));
  QVERIFY(lfIdx >= 0);
  lineEnding->setCurrentIndex(lfIdx);
  input->clear();

  QSignalSpy spy(&w, &SerialMonitorWidget::writeRequested);
  QTest::mouseClick(sendButton, Qt::LeftButton);
  QCOMPARE(spy.count(), 1);
  const QByteArray sent = spy.takeFirst().at(0).toByteArray();
  QCOMPARE(sent, QByteArray("\n"));

  lineEnding->setCurrentIndex(0);  // No line ending
  QTest::mouseClick(sendButton, Qt::LeftButton);
  QCOMPARE(spy.count(), 0);
}

void TestSerialMonitorWidget::sendHistoryUpDownAndPersists() {
  {
    SerialMonitorWidget w;
    w.resize(700, 320);
    w.show();
    w.setConnected(true);

    auto* input = w.findChild<QLineEdit*>("serialMonitorInput");
    QVERIFY(input);
    auto* sendButton = w.findChild<QPushButton*>("serialMonitorSendButton");
    QVERIFY(sendButton);
    auto* lineEnding = w.findChild<QComboBox*>("serialMonitorLineEnding");
    QVERIFY(lineEnding);

    // Newline so "send" always emits and clears.
    const int lfIdx = lineEnding->findData(QByteArray("\n"));
    QVERIFY(lfIdx >= 0);
    lineEnding->setCurrentIndex(lfIdx);

    input->setText("first");
    QTest::mouseClick(sendButton, Qt::LeftButton);
    input->setText("second");
    QTest::mouseClick(sendButton, Qt::LeftButton);

    input->setFocus();
    QTest::keyClick(input, Qt::Key_Up);
    QCOMPARE(input->text(), QString("second"));
    QTest::keyClick(input, Qt::Key_Up);
    QCOMPARE(input->text(), QString("first"));

    QTest::keyClick(input, Qt::Key_Down);
    QCOMPARE(input->text(), QString("second"));
    QTest::keyClick(input, Qt::Key_Down);
    QCOMPARE(input->text(), QString());
  }

  // Recreate widget to confirm persistence.
  {
    SerialMonitorWidget w;
    w.resize(700, 320);
    w.show();
    w.setConnected(true);

    auto* input = w.findChild<QLineEdit*>("serialMonitorInput");
    QVERIFY(input);
    input->setFocus();

    QTest::keyClick(input, Qt::Key_Up);
    QCOMPARE(input->text(), QString("second"));
    QTest::keyClick(input, Qt::Key_Up);
    QCOMPARE(input->text(), QString("first"));
  }
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
  QApplication::setApplicationName("SerialMonitorTest");

  TestSerialMonitorWidget tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_serial_monitor_widget.moc"

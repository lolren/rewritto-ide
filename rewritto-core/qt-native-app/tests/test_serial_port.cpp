#include <QtTest/QtTest>

#include <QCoreApplication>

#include "serial_port.h"

class TestSerialPort final : public QObject {
  Q_OBJECT

 private slots:
  void openInvalidPathEmitsError();
};

void TestSerialPort::openInvalidPathEmitsError() {
  SerialPort port;
  QSignalSpy errorSpy(&port, &SerialPort::errorOccurred);

  const bool ok = port.openPort("/dev/does-not-exist", 115200);
  QVERIFY(!ok);
  QVERIFY(!port.isOpen());
  QVERIFY(errorSpy.count() > 0);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TestSerialPort tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_serial_port.moc"


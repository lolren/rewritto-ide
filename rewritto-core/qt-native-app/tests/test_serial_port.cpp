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

#if defined(Q_OS_WIN)
  const QString invalidPath = "COM0";
#else
  const QString invalidPath = "/dev/does-not-exist";
#endif

  const bool ok = port.openPort(invalidPath, 115200);
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

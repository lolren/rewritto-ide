#pragma once

#include <QObject>

class QSocketNotifier;
class QTimer;

class SerialPort final : public QObject {
  Q_OBJECT

 public:
  explicit SerialPort(QObject* parent = nullptr);

  bool openPort(const QString& portPath, int baudRate);
  void closePort();

  bool isOpen() const;
  QString portPath() const;
  int baudRate() const;

  bool writeBytes(const QByteArray& data);

 signals:
  void openedChanged(bool opened);
  void dataReceived(QByteArray data);
  void errorOccurred(QString message);

 private:
#if defined(Q_OS_UNIX)
  int fd_ = -1;
  QSocketNotifier* readNotifier_ = nullptr;
#elif defined(Q_OS_WIN)
  qintptr nativeHandle_ = -1;
  QTimer* readTimer_ = nullptr;
#endif
  QString portPath_;
  int baudRate_ = 0;

  void setOpen(bool open);
  void handleReadable();
};

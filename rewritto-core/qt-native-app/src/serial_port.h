#pragma once

#include <QObject>

class QSocketNotifier;

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
  int fd_ = -1;
  QString portPath_;
  int baudRate_ = 0;
  QSocketNotifier* readNotifier_ = nullptr;

  void setOpen(bool open);
  void handleReadable();
};


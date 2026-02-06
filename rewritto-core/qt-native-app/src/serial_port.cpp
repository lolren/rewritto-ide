#include "serial_port.h"

#include <QSocketNotifier>

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

static speed_t baudToSpeed(int baudRate) {
  switch (baudRate) {
    case 50:
      return B50;
    case 75:
      return B75;
    case 110:
      return B110;
    case 134:
      return B134;
    case 150:
      return B150;
    case 200:
      return B200;
    case 300:
      return B300;
    case 600:
      return B600;
    case 1200:
      return B1200;
    case 1800:
      return B1800;
    case 2400:
      return B2400;
    case 4800:
      return B4800;
    case 9600:
      return B9600;
    case 19200:
      return B19200;
    case 38400:
      return B38400;
    case 57600:
      return B57600;
    case 115200:
      return B115200;
    case 230400:
      return B230400;
#ifdef B460800
    case 460800:
      return B460800;
#endif
#ifdef B500000
    case 500000:
      return B500000;
#endif
#ifdef B576000
    case 576000:
      return B576000;
#endif
#ifdef B921600
    case 921600:
      return B921600;
#endif
#ifdef B1000000
    case 1000000:
      return B1000000;
#endif
#ifdef B1152000
    case 1152000:
      return B1152000;
#endif
#ifdef B1500000
    case 1500000:
      return B1500000;
#endif
#ifdef B2000000
    case 2000000:
      return B2000000;
#endif
#ifdef B2500000
    case 2500000:
      return B2500000;
#endif
#ifdef B3000000
    case 3000000:
      return B3000000;
#endif
#ifdef B3500000
    case 3500000:
      return B3500000;
#endif
#ifdef B4000000
    case 4000000:
      return B4000000;
#endif
    default:
      return 0;
  }
}

SerialPort::SerialPort(QObject* parent) : QObject(parent) {}

bool SerialPort::openPort(const QString& portPath, int baudRate) {
  closePort();

  const QByteArray pathBytes = portPath.toLocal8Bit();
  const int fd = ::open(pathBytes.constData(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) {
    emit errorOccurred(
        QString("Failed to open %1: %2").arg(portPath, QString::fromLocal8Bit(strerror(errno))));
    return false;
  }

  const speed_t speed = baudToSpeed(baudRate);
  if (speed == 0) {
    ::close(fd);
    emit errorOccurred(QString("Unsupported baud rate: %1").arg(baudRate));
    return false;
  }

  termios tty {};
  if (tcgetattr(fd, &tty) != 0) {
    ::close(fd);
    emit errorOccurred(QString("tcgetattr failed: %1").arg(QString::fromLocal8Bit(strerror(errno))));
    return false;
  }

  cfmakeraw(&tty);
  tty.c_cflag |= (CLOCAL | CREAD);
  tty.c_cflag &= ~CRTSCTS;
  tty.c_cflag &= ~PARENB;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 0;

  if (cfsetispeed(&tty, speed) != 0 || cfsetospeed(&tty, speed) != 0) {
    ::close(fd);
    emit errorOccurred(QString("Failed to set baud rate: %1").arg(QString::fromLocal8Bit(strerror(errno))));
    return false;
  }

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    ::close(fd);
    emit errorOccurred(QString("tcsetattr failed: %1").arg(QString::fromLocal8Bit(strerror(errno))));
    return false;
  }

  fd_ = fd;
  portPath_ = portPath;
  baudRate_ = baudRate;

  readNotifier_ = new QSocketNotifier(fd_, QSocketNotifier::Read, this);
  connect(readNotifier_, &QSocketNotifier::activated, this,
          [this](QSocketDescriptor, QSocketNotifier::Type) { handleReadable(); });

  setOpen(true);
  return true;
}

void SerialPort::closePort() {
  if (readNotifier_) {
    readNotifier_->setEnabled(false);
    readNotifier_->deleteLater();
    readNotifier_ = nullptr;
  }
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
  if (!portPath_.isEmpty() || baudRate_ != 0) {
    portPath_.clear();
    baudRate_ = 0;
    setOpen(false);
  }
}

bool SerialPort::isOpen() const {
  return fd_ >= 0;
}

QString SerialPort::portPath() const {
  return portPath_;
}

int SerialPort::baudRate() const {
  return baudRate_;
}

bool SerialPort::writeBytes(const QByteArray& data) {
  if (fd_ < 0) {
    emit errorOccurred("Serial port is not open.");
    return false;
  }
  if (data.isEmpty()) {
    return true;
  }
  const char* p = data.constData();
  qsizetype remaining = data.size();
  while (remaining > 0) {
    const ssize_t n = ::write(fd_, p, static_cast<size_t>(remaining));
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Non-blocking; best effort.
        break;
      }
      emit errorOccurred(QString("Write failed: %1").arg(QString::fromLocal8Bit(strerror(errno))));
      return false;
    }
    p += n;
    remaining -= n;
  }
  return true;
}

void SerialPort::setOpen(bool open) {
  emit openedChanged(open);
}

void SerialPort::handleReadable() {
  if (fd_ < 0) {
    return;
  }

  QByteArray data;
  data.resize(4096);

  while (true) {
    const ssize_t n = ::read(fd_, data.data(), static_cast<size_t>(data.size()));
    if (n > 0) {
      emit dataReceived(data.left(static_cast<int>(n)));
      continue;
    }
    if (n == 0) {
      emit errorOccurred("Serial port closed.");
      closePort();
      return;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
    emit errorOccurred(QString("Read failed: %1").arg(QString::fromLocal8Bit(strerror(errno))));
    closePort();
    return;
  }
}

#include "serial_port.h"

#if defined(Q_OS_UNIX)
#include <QSocketNotifier>

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>
#include <limits>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#if defined(Q_OS_UNIX)
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
#elif defined(Q_OS_WIN)
static QString windowsErrorString(const QString& context, DWORD errorCode = GetLastError()) {
  LPWSTR buffer = nullptr;
  const DWORD flags =
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  const DWORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
  const DWORD chars =
      FormatMessageW(flags, nullptr, errorCode, lang, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);

  QString message;
  if (chars > 0 && buffer != nullptr) {
    message = QString::fromWCharArray(buffer, static_cast<int>(chars)).trimmed();
  }
  if (buffer != nullptr) {
    LocalFree(buffer);
  }
  if (message.isEmpty()) {
    message = QString("Windows error %1").arg(errorCode);
  }
  return QString("%1: %2").arg(context, message);
}

static QString normalizeWindowsPortPath(const QString& rawPortPath) {
  const QString path = rawPortPath.trimmed();
  if (path.startsWith("\\\\.\\")) {
    return path;
  }

  static const QRegularExpression kComRegex("^COM(\\d+)$",
                                             QRegularExpression::CaseInsensitiveOption);
  const QRegularExpressionMatch match = kComRegex.match(path);
  if (!match.hasMatch()) {
    return path;
  }

  bool ok = false;
  const int portNumber = match.captured(1).toInt(&ok);
  if (!ok) {
    return path.toUpper();
  }
  if (portNumber > 9) {
    return QStringLiteral("\\\\.\\") + path.toUpper();
  }
  return path.toUpper();
}

static HANDLE handleFromNative(qintptr nativeHandle) {
  return reinterpret_cast<HANDLE>(nativeHandle);
}

static bool isValidNativeHandle(qintptr nativeHandle) {
  const HANDLE handle = handleFromNative(nativeHandle);
  return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}
#endif

SerialPort::SerialPort(QObject* parent) : QObject(parent) {}

bool SerialPort::openPort(const QString& portPath, int baudRate) {
#if !defined(Q_OS_UNIX) && !defined(Q_OS_WIN)
  Q_UNUSED(portPath);
  Q_UNUSED(baudRate);
  closePort();
  emit errorOccurred("Serial port is not supported on this platform.");
  return false;
#elif defined(Q_OS_UNIX)
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
#else
  closePort();

  const QString trimmedPortPath = portPath.trimmed();
  if (trimmedPortPath.isEmpty()) {
    emit errorOccurred("Serial port path is empty.");
    return false;
  }

  const QString normalizedPath = normalizeWindowsPortPath(trimmedPortPath);
  HANDLE handle =
      CreateFileW(reinterpret_cast<LPCWSTR>(normalizedPath.utf16()), GENERIC_READ | GENERIC_WRITE, 0,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    emit errorOccurred(windowsErrorString(QString("Failed to open %1").arg(trimmedPortPath)));
    return false;
  }

  DCB dcb {};
  dcb.DCBlength = sizeof(DCB);
  if (!GetCommState(handle, &dcb)) {
    const QString error = windowsErrorString("GetCommState failed");
    CloseHandle(handle);
    emit errorOccurred(error);
    return false;
  }

  dcb.BaudRate = static_cast<DWORD>(baudRate);
  dcb.ByteSize = 8;
  dcb.Parity = NOPARITY;
  dcb.StopBits = ONESTOPBIT;
  dcb.fBinary = TRUE;
  dcb.fParity = FALSE;
  dcb.fOutxCtsFlow = FALSE;
  dcb.fOutxDsrFlow = FALSE;
  dcb.fDtrControl = DTR_CONTROL_ENABLE;
  dcb.fRtsControl = RTS_CONTROL_ENABLE;
  dcb.fOutX = FALSE;
  dcb.fInX = FALSE;

  if (!SetCommState(handle, &dcb)) {
    const QString error = windowsErrorString("SetCommState failed");
    CloseHandle(handle);
    emit errorOccurred(error);
    return false;
  }

  COMMTIMEOUTS timeouts {};
  timeouts.ReadIntervalTimeout = MAXDWORD;
  timeouts.ReadTotalTimeoutMultiplier = 0;
  timeouts.ReadTotalTimeoutConstant = 0;
  timeouts.WriteTotalTimeoutMultiplier = 0;
  timeouts.WriteTotalTimeoutConstant = 1000;
  if (!SetCommTimeouts(handle, &timeouts)) {
    const QString error = windowsErrorString("SetCommTimeouts failed");
    CloseHandle(handle);
    emit errorOccurred(error);
    return false;
  }

  SetupComm(handle, 4096, 4096);
  PurgeComm(handle, PURGE_RXABORT | PURGE_TXABORT | PURGE_RXCLEAR | PURGE_TXCLEAR);

  nativeHandle_ = reinterpret_cast<qintptr>(handle);
  portPath_ = trimmedPortPath;
  baudRate_ = baudRate;

  readTimer_ = new QTimer(this);
  readTimer_->setInterval(15);
  connect(readTimer_, &QTimer::timeout, this, &SerialPort::handleReadable);
  readTimer_->start();

  setOpen(true);
  return true;
#endif
}

void SerialPort::closePort() {
#if defined(Q_OS_UNIX)
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
#elif defined(Q_OS_WIN)
  if (readTimer_) {
    readTimer_->stop();
    readTimer_->deleteLater();
    readTimer_ = nullptr;
  }
  if (isValidNativeHandle(nativeHandle_)) {
    CloseHandle(handleFromNative(nativeHandle_));
  }
  nativeHandle_ = -1;
  if (!portPath_.isEmpty() || baudRate_ != 0) {
    portPath_.clear();
    baudRate_ = 0;
    setOpen(false);
  }
#else
  if (!portPath_.isEmpty() || baudRate_ != 0) {
    portPath_.clear();
    baudRate_ = 0;
    setOpen(false);
  }
#endif
}

bool SerialPort::isOpen() const {
#if defined(Q_OS_UNIX)
  return fd_ >= 0;
#elif defined(Q_OS_WIN)
  return isValidNativeHandle(nativeHandle_);
#else
  return false;
#endif
}

QString SerialPort::portPath() const {
  return portPath_;
}

int SerialPort::baudRate() const {
  return baudRate_;
}

bool SerialPort::writeBytes(const QByteArray& data) {
#if !defined(Q_OS_UNIX) && !defined(Q_OS_WIN)
  Q_UNUSED(data);
  emit errorOccurred("Serial port is not supported on this platform.");
  return false;
#elif defined(Q_OS_UNIX)
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
#else
  if (!isOpen()) {
    emit errorOccurred("Serial port is not open.");
    return false;
  }
  if (data.isEmpty()) {
    return true;
  }

  HANDLE handle = handleFromNative(nativeHandle_);
  const char* p = data.constData();
  qsizetype remaining = data.size();
  while (remaining > 0) {
    const DWORD chunk =
        static_cast<DWORD>(std::min<qsizetype>(remaining, std::numeric_limits<DWORD>::max()));
    DWORD written = 0;
    if (!WriteFile(handle, p, chunk, &written, nullptr)) {
      emit errorOccurred(windowsErrorString("Write failed"));
      return false;
    }
    if (written == 0) {
      emit errorOccurred("Write failed: no bytes were written.");
      return false;
    }
    p += static_cast<qsizetype>(written);
    remaining -= static_cast<qsizetype>(written);
  }
  return true;
#endif
}

void SerialPort::setOpen(bool open) {
  emit openedChanged(open);
}

void SerialPort::handleReadable() {
#if !defined(Q_OS_UNIX) && !defined(Q_OS_WIN)
  return;
#elif defined(Q_OS_UNIX)
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
#elif defined(Q_OS_WIN)
  if (!isOpen()) {
    return;
  }

  HANDLE handle = handleFromNative(nativeHandle_);
  for (int i = 0; i < 16; ++i) {
    DWORD errors = 0;
    COMSTAT stat {};
    if (!ClearCommError(handle, &errors, &stat)) {
      emit errorOccurred(windowsErrorString("ClearCommError failed"));
      closePort();
      return;
    }
    if (stat.cbInQue == 0) {
      return;
    }

    const DWORD bytesToRead = std::min<DWORD>(stat.cbInQue, 4096);
    QByteArray data;
    data.resize(static_cast<int>(bytesToRead));
    DWORD bytesRead = 0;
    if (!ReadFile(handle, data.data(), bytesToRead, &bytesRead, nullptr)) {
      emit errorOccurred(windowsErrorString("Read failed"));
      closePort();
      return;
    }
    if (bytesRead == 0) {
      return;
    }
    emit dataReceived(data.left(static_cast<int>(bytesRead)));
  }
#endif
}

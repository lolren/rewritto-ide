#include "serial_monitor_widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QTextBlock>
#include <QTextCursor>
#include <QTime>
#include <QVBoxLayout>

namespace {
QString normalizedNewlines(QString text) {
  text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
  return text;
}

QString timestampPrefix() {
  return QStringLiteral("[") + QTime::currentTime().toString("HH:mm:ss.zzz") +
         QStringLiteral("] ");
}
}  // namespace

SerialMonitorWidget::SerialMonitorWidget(QWidget* parent) : QWidget(parent) {
  connectButton_ = new QPushButton(tr("Connect"), this);
  connectButton_->setCheckable(true);

  portLabel_ = new QLabel(tr("Port: (none)"), this);

  baudCombo_ = new QComboBox(this);
  const QVector<int> baudRates = {300,   600,   1200,  2400,   4800,   9600,  14400,
                                  19200, 28800, 38400, 57600,  115200, 230400, 460800,
                                  921600};
  for (int b : baudRates) {
    baudCombo_->addItem(QString::number(b), b);
  }
  baudCombo_->setCurrentText("115200");

  lineEndingCombo_ = new QComboBox(this);
  lineEndingCombo_->setObjectName("serialMonitorLineEnding");
  lineEndingCombo_->addItem(tr("No line ending"), QByteArray());
  lineEndingCombo_->addItem(tr("Newline (LF)"), QByteArray("\n"));
  lineEndingCombo_->addItem(tr("Carriage return (CR)"), QByteArray("\r"));
  lineEndingCombo_->addItem(tr("Both CR + LF"), QByteArray("\r\n"));

  autoScrollCheck_ = new QCheckBox(tr("Autoscroll"), this);
  autoScrollCheck_->setChecked(true);
  autoScrollCheck_->setObjectName("serialMonitorAutoscroll");

  timestampsCheck_ = new QCheckBox(tr("Timestamp"), this);
  timestampsCheck_->setChecked(false);
  timestampsCheck_->setObjectName("serialMonitorTimestamps");

  autoReconnectCheck_ = new QCheckBox(tr("Auto reconnect"), this);
  autoReconnectCheck_->setChecked(false);
  autoReconnectCheck_->setObjectName("serialMonitorAutoReconnect");

  saveButton_ = new QPushButton(tr("Save"), this);

  clearButton_ = new QPushButton(tr("Clear"), this);

  statusLabel_ = new QLabel(tr("Disconnected"), this);

  output_ = new QPlainTextEdit(this);
  output_->setObjectName("serialMonitorOutput");
  output_->setReadOnly(true);
  output_->setMaximumBlockCount(5000);

  input_ = new QLineEdit(this);
  input_->setObjectName("serialMonitorInput");
  input_->setPlaceholderText(tr("Type to send…"));

  sendButton_ = new QPushButton(tr("Send"), this);
  sendButton_->setObjectName("serialMonitorSendButton");

  auto* topRow = new QHBoxLayout();
  topRow->addWidget(connectButton_);
  topRow->addWidget(portLabel_);
  topRow->addStretch(1);
  topRow->addWidget(new QLabel(tr("Baud:"), this));
  topRow->addWidget(baudCombo_);
  topRow->addSpacing(12);
  topRow->addWidget(new QLabel(tr("Line ending:"), this));
  topRow->addWidget(lineEndingCombo_);
  topRow->addSpacing(12);
  topRow->addWidget(autoScrollCheck_);
  topRow->addWidget(timestampsCheck_);
  topRow->addWidget(autoReconnectCheck_);
  topRow->addWidget(saveButton_);
  topRow->addWidget(clearButton_);

  auto* bottomRow = new QHBoxLayout();
  bottomRow->addWidget(input_, 1);
  bottomRow->addWidget(sendButton_);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(6, 6, 6, 6);
  layout->addLayout(topRow);
  layout->addWidget(statusLabel_);
  layout->addWidget(output_, 1);
  layout->addLayout(bottomRow);

  {
    QSettings settings;
    settings.beginGroup("SerialMonitor");
    const bool autoscroll = settings.value("autoscroll", true).toBool();
    const bool timestamps = settings.value("timestamps", false).toBool();
    const bool autoReconnect = settings.value("autoReconnect", false).toBool();
    const int baud = settings.value("baud", 115200).toInt();
    const int endingIndex = settings.value("lineEndingIndex", 0).toInt();
    settings.endGroup();

    autoScrollCheck_->setChecked(autoscroll);
    timestampsCheck_->setChecked(timestamps);
    autoReconnectCheck_->setChecked(autoReconnect);
    const int baudIdx = baudCombo_->findData(baud);
    if (baudIdx >= 0) {
      baudCombo_->setCurrentIndex(baudIdx);
    }
    if (endingIndex >= 0 && endingIndex < lineEndingCombo_->count()) {
      lineEndingCombo_->setCurrentIndex(endingIndex);
    }
  }

  auto persistSettings = [this] {
    QSettings settings;
    settings.beginGroup("SerialMonitor");
    settings.setValue("autoscroll", autoScrollCheck_->isChecked());
    settings.setValue("timestamps", timestampsCheck_->isChecked());
    settings.setValue("autoReconnect", autoReconnectCheck_->isChecked());
    settings.setValue("baud", baudCombo_->currentData().toInt());
    settings.setValue("lineEndingIndex", lineEndingCombo_->currentIndex());
    settings.endGroup();
  };

  connect(autoScrollCheck_, &QCheckBox::toggled, this,
          [persistSettings](bool) { persistSettings(); });
  connect(timestampsCheck_, &QCheckBox::toggled, this,
          [persistSettings](bool) { persistSettings(); });
  connect(autoReconnectCheck_, &QCheckBox::toggled, this,
          [persistSettings](bool) { persistSettings(); });
  connect(baudCombo_, &QComboBox::currentIndexChanged, this,
          [persistSettings](int) { persistSettings(); });
  connect(lineEndingCombo_, &QComboBox::currentIndexChanged, this,
          [persistSettings](int) { persistSettings(); });

  connect(clearButton_, &QPushButton::clicked, this, [this] {
    atLineStart_ = true;
    lineText_.clear();
    lineCursor_ = 0;
    lineContentStart_ = 0;
    output_->clear();
  });
  connect(saveButton_, &QPushButton::clicked, this, [this] {
    const QString stamp =
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString initial =
        QDir::homePath() +
        QStringLiteral("/serial-monitor-%1.txt").arg(stamp);
    const QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Serial Log"), initial,
        tr("Text Files (*.txt);;All Files (*)"));
    if (filePath.isEmpty()) {
      return;
    }
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
      showError(tr("Could not write log file."));
      return;
    }
    f.write(output_->toPlainText().toUtf8());
    f.close();
    statusLabel_->setText(tr("Saved log to %1").arg(QFileInfo(filePath).fileName()));
  });
  connect(sendButton_, &QPushButton::clicked, this, [this] { emitSend(); });
  connect(input_, &QLineEdit::returnPressed, this, [this] { emitSend(); });

  connect(connectButton_, &QPushButton::toggled, this, [this](bool checked) {
    if (checked) {
      const int baud = baudCombo_->currentData().toInt();
      emit connectRequested(currentPort_, baud);
    } else {
      emit disconnectRequested();
    }
  });

  setConnected(false);

  {
    QSettings settings;
    settings.beginGroup("SerialMonitor");
    sendHistory_ = settings.value("sendHistory").toStringList();
    settings.endGroup();
  }
  sendHistory_.removeAll(QString{});
  for (QString& s : sendHistory_) {
    s.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    s.replace(QLatin1Char('\r'), QLatin1Char('\n'));
  }
  while (!sendHistory_.isEmpty() && sendHistory_.first().trimmed().isEmpty()) {
    sendHistory_.removeFirst();
  }
  sendHistoryIndex_ = sendHistory_.size();
  sendHistoryDraft_.clear();

  if (input_) {
    input_->installEventFilter(this);
  }
}

void SerialMonitorWidget::setCurrentPort(QString port) {
  currentPort_ = std::move(port);
  portLabel_->setText(currentPort_.isEmpty() ? tr("Port: (none)")
                                            : tr("Port: %1").arg(currentPort_));
  if (!connectButton_->isChecked()) {
    connectButton_->setEnabled(!currentPort_.isEmpty());
  }
}

QString SerialMonitorWidget::currentPort() const {
  return currentPort_;
}

bool SerialMonitorWidget::autoReconnectEnabled() const {
  return autoReconnectCheck_ && autoReconnectCheck_->isChecked();
}

void SerialMonitorWidget::setConnected(bool connected) {
  connectButton_->blockSignals(true);
  connectButton_->setChecked(connected);
  connectButton_->blockSignals(false);

  connectButton_->setText(connected ? tr("Disconnect") : tr("Connect"));
  connectButton_->setEnabled(connected || !currentPort_.isEmpty());
  baudCombo_->setEnabled(!connected);
  input_->setEnabled(connected);
  sendButton_->setEnabled(connected);
  statusLabel_->setText(connected ? tr("Connected") : tr("Disconnected"));
}

void SerialMonitorWidget::appendData(QByteArray data) {
  if (data.isEmpty()) {
    return;
  }

  if (!output_) {
    return;
  }

  QString text = QString::fromUtf8(data.constData(), data.size());
  if (text.isEmpty()) {
    return;
  }

  QTextDocument* doc = output_->document();
  bool dirtyLine = false;

  auto commitLineText = [this, doc] {
    if (!doc) {
      return;
    }
    const QTextBlock block = doc->lastBlock();
    const int start = block.position();
    const int end = start + block.text().size();
    QTextCursor c(doc);
    c.setPosition(start);
    c.setPosition(end, QTextCursor::KeepAnchor);
    c.insertText(lineText_);
  };

  auto ensureTimestampPrefix = [this, &dirtyLine] {
    if (!timestampsCheck_ || !timestampsCheck_->isChecked()) {
      return;
    }
    if (!atLineStart_) {
      return;
    }
    lineText_ += timestampPrefix();
    lineContentStart_ = lineText_.size();
    lineCursor_ = lineText_.size();
    atLineStart_ = false;
    dirtyLine = true;
  };

  for (int i = 0; i < text.size(); ++i) {
    const QChar ch = text.at(i);
    if (ch == QLatin1Char('\r')) {
      if (!atLineStart_) {
        lineCursor_ = lineContentStart_;
      }
      continue;
    }

    if (ch == QLatin1Char('\n')) {
      ensureTimestampPrefix();
      if (dirtyLine) {
        commitLineText();
        dirtyLine = false;
      }

      QTextCursor c(doc);
      c.movePosition(QTextCursor::End);
      c.insertText(QStringLiteral("\n"));

      atLineStart_ = true;
      lineText_.clear();
      lineCursor_ = 0;
      lineContentStart_ = 0;
      continue;
    }

    ensureTimestampPrefix();
    if (atLineStart_) {
      atLineStart_ = false;
    }

    if (lineCursor_ < 0) {
      lineCursor_ = 0;
    }
    if (lineCursor_ > lineText_.size()) {
      lineCursor_ = lineText_.size();
    }

    if (lineCursor_ < lineText_.size()) {
      lineText_[lineCursor_] = ch;
    } else {
      lineText_.append(ch);
    }
    ++lineCursor_;
    dirtyLine = true;
  }

  if (dirtyLine) {
    commitLineText();
  }

  if (autoScrollCheck_->isChecked()) {
    if (auto* sb = output_->verticalScrollBar()) {
      sb->setValue(sb->maximum());
    }
  }
}

void SerialMonitorWidget::showError(QString message) {
  if (message.trimmed().isEmpty()) {
    return;
  }
  statusLabel_->setText(tr("Error: %1").arg(message));
}

bool SerialMonitorWidget::eventFilter(QObject* watched, QEvent* event) {
  if (watched == input_ && event && event->type() == QEvent::KeyPress) {
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down) {
      const Qt::KeyboardModifiers mods = key->modifiers();
      if (mods != Qt::NoModifier && mods != Qt::KeypadModifier) {
        return QWidget::eventFilter(watched, event);
      }
      if (!input_ || sendHistory_.isEmpty()) {
        return QWidget::eventFilter(watched, event);
      }

      if (key->key() == Qt::Key_Up) {
        if (sendHistoryIndex_ >= sendHistory_.size()) {
          sendHistoryDraft_ = input_->text();
        }
        if (sendHistoryIndex_ > 0) {
          --sendHistoryIndex_;
        }
        const QString entry = sendHistory_.value(sendHistoryIndex_);
        input_->setText(entry);
        input_->setCursorPosition(entry.size());
        return true;
      }

      // Down
      if (sendHistoryIndex_ < sendHistory_.size()) {
        ++sendHistoryIndex_;
      }
      if (sendHistoryIndex_ >= sendHistory_.size()) {
        input_->setText(sendHistoryDraft_);
        input_->setCursorPosition(input_->text().size());
        return true;
      }
      const QString entry = sendHistory_.value(sendHistoryIndex_);
      input_->setText(entry);
      input_->setCursorPosition(entry.size());
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void SerialMonitorWidget::emitSend() {
  const QString text = input_ ? input_->text() : QString{};

  QByteArray data = text.toUtf8();
  data.append(currentLineEndingBytes());
  if (data.isEmpty()) {
    return;
  }

  if (!text.isEmpty()) {
    appendToSendHistory(text);
  }

  emit writeRequested(data);
  if (input_) {
    input_->clear();
  }

  sendHistoryIndex_ = sendHistory_.size();
  sendHistoryDraft_.clear();
}

QByteArray SerialMonitorWidget::currentLineEndingBytes() const {
  return lineEndingCombo_->currentData().toByteArray();
}

void SerialMonitorWidget::appendToSendHistory(QString entry) {
  entry = normalizedNewlines(std::move(entry));
  if (entry.trimmed().isEmpty()) {
    return;
  }
  if (!sendHistory_.isEmpty() && sendHistory_.last() == entry) {
    return;
  }

  sendHistory_.push_back(entry);
  const int maxItems = 50;
  while (sendHistory_.size() > maxItems) {
    sendHistory_.removeFirst();
  }
  sendHistoryIndex_ = sendHistory_.size();

  QSettings settings;
  settings.beginGroup("SerialMonitor");
  settings.setValue("sendHistory", sendHistory_);
  settings.endGroup();
}

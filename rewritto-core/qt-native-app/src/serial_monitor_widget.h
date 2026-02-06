#pragma once

#include <QWidget>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTextDocument;

class SerialMonitorWidget final : public QWidget {
  Q_OBJECT

 public:
  explicit SerialMonitorWidget(QWidget* parent = nullptr);

  void setCurrentPort(QString port);
  QString currentPort() const;
  bool autoReconnectEnabled() const;

 public slots:
  void setConnected(bool connected);
  void appendData(QByteArray data);
  void showError(QString message);

 signals:
  void connectRequested(QString port, int baudRate);
  void disconnectRequested();
  void writeRequested(QByteArray data);

 private:
  QString currentPort_;
  bool atLineStart_ = true;
  QString lineText_;
  int lineCursor_ = 0;        // 0-based column within lineText_
  int lineContentStart_ = 0;  // after timestamp prefix (if present)
  QStringList sendHistory_;
  int sendHistoryIndex_ = 0;  // [0..sendHistory_.size()]
  QString sendHistoryDraft_;

  QPushButton* connectButton_ = nullptr;
  QLabel* portLabel_ = nullptr;
  QComboBox* baudCombo_ = nullptr;
  QComboBox* lineEndingCombo_ = nullptr;
  QCheckBox* autoScrollCheck_ = nullptr;
  QCheckBox* timestampsCheck_ = nullptr;
  QCheckBox* autoReconnectCheck_ = nullptr;
  QPushButton* saveButton_ = nullptr;
  QPushButton* clearButton_ = nullptr;
  QLineEdit* filterInput_ = nullptr;
  QPlainTextEdit* output_ = nullptr;
  QTextDocument* rawOutputDocument_ = nullptr;
  QTextDocument* filteredOutputDocument_ = nullptr;
  QLineEdit* input_ = nullptr;
  QPushButton* sendButton_ = nullptr;
  QLabel* statusLabel_ = nullptr;

  bool eventFilter(QObject* watched, QEvent* event) override;

  void emitSend();
  QByteArray currentLineEndingBytes() const;
  void appendToSendHistory(QString entry);
  void applyOutputFilter();
};

#pragma once

#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>

class ArduinoCli final : public QObject {
  Q_OBJECT

 public:
  explicit ArduinoCli(QObject* parent = nullptr);

  void setArduinoCliPath(QString path);
  QString arduinoCliPath() const;
  QString arduinoCliConfigPath() const;
  QStringList withGlobalFlags(QStringList args) const;

  bool isRunning() const;
  void stop();

  void run(QStringList args, QString workingDirectory = {});

 signals:
  void started();
  void outputReceived(QString text);
  void diagnosticFound(QString filePath,
                       int line,
                       int column,
                       QString severity,
                       QString message);
  void finished(int exitCode, QProcess::ExitStatus exitStatus);

 private:
  struct PendingDiagnostic final {
    QString filePath;
    int line = 0;
    int column = 0;
    QString severity;
    QString message;
    int extraLines = 0;
  };

  QString arduinoCliPath_;
  QString arduinoCliConfigPath_;
  QProcess* process_ = nullptr;
  QString lineBuffer_;
  QRegularExpression diagnosticWithColumn_;
  QRegularExpression diagnosticNoColumn_;
  QRegularExpression toolDiagnostic_;
  QRegularExpression libraryConflictStart_;
  QRegularExpression collect2Error_;
  QRegularExpression undefinedReferenceObjectLine_;
  QRegularExpression undefinedReferenceWithLine_;
  QRegularExpression ldCannotFindLine_;
  QRegularExpression exitStatusLine_;
  QRegularExpression compilationErrorLine_;

  bool hasPendingDiagnostic_ = false;
  PendingDiagnostic pendingDiagnostic_;
  QStringList includeContextLines_;

  void consumeText(const QString& chunk);
  void consumeLine(QString line);
  void flushPendingDiagnostic();

  static QString resolveDefaultArduinoCliPath();
  static QString resolveDefaultArduinoCliConfigPath();
};

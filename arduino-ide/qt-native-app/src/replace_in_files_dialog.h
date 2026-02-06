#pragma once

#include <QWidget>

#include <atomic>

class QLabel;
class QLineEdit;
class QPushButton;
class QThread;
class QTreeWidget;

class ReplaceInFilesWorker final : public QObject {
  Q_OBJECT

 public:
  explicit ReplaceInFilesWorker(QObject* parent = nullptr);

 public slots:
  void preview(QString rootDir,
               QString query,
               QStringList patterns,
               bool caseSensitive,
               bool wholeWord);
  void apply(QString rootDir,
             QString query,
             QString replaceText,
             QStringList patterns,
             bool caseSensitive,
             bool wholeWord);
  void cancel();

 signals:
  void matchFound(QString filePath, int line, int column, QString preview);
  void previewFinished(int matches, int filesScanned);
  void applyFinished(int matchesReplaced, int filesScanned, QStringList modifiedFiles);
  void message(QString text);

 private:
	std::atomic_bool cancelled_{false};
};

class ReplaceInFilesDialog final : public QWidget {
  Q_OBJECT

 public:
  explicit ReplaceInFilesDialog(QString rootDir, QWidget* parent = nullptr);
  ~ReplaceInFilesDialog() override;

  void setRootDir(QString rootDir);
  void setQueryText(QString text);
  void setReplaceText(QString text);
  void focusQuery();

 signals:
  void openLocation(QString filePath, int line, int column);
  void filesModified(QStringList filePaths);

 private:
  QString rootDir_;

  QLineEdit* queryEdit_ = nullptr;
  QLineEdit* replaceEdit_ = nullptr;
  QLineEdit* patternsEdit_ = nullptr;
  class QCheckBox* caseSensitive_ = nullptr;
  class QCheckBox* wholeWord_ = nullptr;
  QPushButton* findButton_ = nullptr;
  QPushButton* replaceAllButton_ = nullptr;
  QPushButton* cancelButton_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QTreeWidget* results_ = nullptr;

  QThread* workerThread_ = nullptr;
  ReplaceInFilesWorker* worker_ = nullptr;

  bool running_ = false;
  int lastMatches_ = 0;
  int lastFilesScanned_ = 0;

  void startPreview();
  void startReplaceAll();
  void stopWork();
  QStringList parsePatterns() const;
  void addResult(QString filePath, int line, int column, QString preview);
  void setRunning(bool running);
};

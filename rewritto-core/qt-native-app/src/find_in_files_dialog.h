#pragma once

#include <QWidget>

#include <atomic>

class QLabel;
class QLineEdit;
class QPushButton;
class QThread;
class QTreeWidget;

class FindInFilesWorker final : public QObject {
  Q_OBJECT

 public:
  explicit FindInFilesWorker(QObject* parent = nullptr);

 public slots:
  void run(QString rootDir,
           QString query,
           QStringList patterns,
           QStringList excludePatterns,
           bool caseSensitive);
  void cancel();

 signals:
  void matchFound(QString filePath, int line, int column, QString preview);
  void finished(int matches, int filesScanned);
  void message(QString text);

 private:
	std::atomic_bool cancelled_{false};
};

class FindInFilesDialog final : public QWidget {
  Q_OBJECT

 public:
  explicit FindInFilesDialog(QString rootDir, QWidget* parent = nullptr);
  ~FindInFilesDialog() override;

  void setRootDir(QString rootDir);
  void setQueryText(QString text);
  void focusQuery();

 signals:
  void openLocation(QString filePath, int line, int column);

 private:
  QString rootDir_;

  QLineEdit* queryEdit_ = nullptr;
  QLineEdit* patternsEdit_ = nullptr;
  QLineEdit* excludeEdit_ = nullptr;
  class QCheckBox* caseSensitive_ = nullptr;
  QPushButton* findButton_ = nullptr;
  QPushButton* cancelButton_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QTreeWidget* results_ = nullptr;

  QThread* workerThread_ = nullptr;
  FindInFilesWorker* worker_ = nullptr;
  int matches_ = 0;
  int filesScanned_ = 0;

  void startSearch();
  void stopSearch();
  QStringList parsePatterns() const;
  QStringList parseExcludePatterns() const;
  void addResult(QString filePath, int line, int column, QString preview);
};

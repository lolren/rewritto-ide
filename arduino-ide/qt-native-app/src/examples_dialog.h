#pragma once

#include <QDialog>

#include "examples_scanner.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QSortFilterProxyModel;
class QStandardItemModel;
class QThread;
class QTreeView;

class QPlainTextEdit;
class QSplitter;

class ExamplesDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit ExamplesDialog(QWidget* parent = nullptr);
  ~ExamplesDialog() override;

  void setScanOptions(ExamplesScanner::Options options);
  void setFilterText(QString text);

 signals:
  void openExampleRequested(QString folderPath, QString inoPath);

 private:
  ExamplesScanner::Options options_;

  QLineEdit* filterEdit_ = nullptr;
  QPushButton* refreshButton_ = nullptr;
  QTreeView* tree_ = nullptr;
  QStandardItemModel* model_ = nullptr;
  QSortFilterProxyModel* proxy_ = nullptr;
  QPlainTextEdit* preview_ = nullptr;
  QPushButton* openButton_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QTimer* previewTimer_ = nullptr;

  QThread* scanThread_ = nullptr;

  void buildUi();
  void wireSignals();

  void startScan();
  void populate(const QVector<ExampleSketch>& examples);
  bool currentSelectionToExample(QString* folderPath, QString* inoPath) const;
  void openSelectedExample();
  void updatePreview();

  static bool copyDirectoryRecursively(const QString& srcDir,
                                      const QString& dstDir,
                                      QString* errorMessage);
};

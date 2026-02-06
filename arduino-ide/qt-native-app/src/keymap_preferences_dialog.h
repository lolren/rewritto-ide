#pragma once

#include <QDialog>
#include <QKeySequence>
#include <QString>
#include <QVector>

class KeymapManager;
class QFilterLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QLabel;
class QShortcut;

class KeymapPreferencesDialog : public QDialog {
  Q_OBJECT

 public:
  explicit KeymapPreferencesDialog(KeymapManager* manager, QWidget* parent = nullptr);
  ~KeymapPreferencesDialog() override;

 private slots:
  void onItemSelectionChanged();
  void onCaptureButtonClicked();
  void onResetButtonClicked();
  void onResetAllButtonClicked();
  void onExportButtonClicked();
  void onImportButtonClicked();
  void onFilterTextChanged(const QString& text);

  void onKeySequenceCaptured(const QKeySequence& seq);

 private:
  void setupUi();
  void populateTree();
  void updateSelectionState();
  void updateConflictWarning(const QString& conflictId = QString());

  KeymapManager* manager_ = nullptr;
  QTreeWidget* tree_ = nullptr;
  FilterLineEdit* filterEdit_ = nullptr;
  QLabel* currentKeyLabel_ = nullptr;
  QPushButton* captureButton_ = nullptr;
  QPushButton* resetButton_ = nullptr;
  QPushButton* resetAllButton_ = nullptr;
  QPushButton* exportButton_ = nullptr;
  QPushButton* importButton_ = nullptr;
  QLabel* conflictLabel_ = nullptr;
  QLabel* statusLabel_ = nullptr;

  QString currentEntryId_;
  bool capturing_ = false;
  QShortcut* captureShortcut_ = nullptr;
};

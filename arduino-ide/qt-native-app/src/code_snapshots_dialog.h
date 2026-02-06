#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QSortFilterProxyModel;
class QStandardItemModel;
class QTableView;
class QPushButton;

class CodeSnapshotsDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit CodeSnapshotsDialog(QString sketchFolder, QWidget* parent = nullptr);

  void reload();
  QString selectedSnapshotId() const;

 signals:
  void captureRequested();
  void restoreRequested(QString snapshotId);
  void editCommentRequested(QString snapshotId, QString currentComment);
  void deleteRequested(QString snapshotId);

 private:
  QString sketchFolder_;
  QLineEdit* filterEdit_ = nullptr;
  QTableView* table_ = nullptr;
  QStandardItemModel* model_ = nullptr;
  QSortFilterProxyModel* proxy_ = nullptr;
  QPushButton* restoreButton_ = nullptr;
  QPushButton* editCommentButton_ = nullptr;
  QPushButton* deleteButton_ = nullptr;

  void updateSelectionActions();
};


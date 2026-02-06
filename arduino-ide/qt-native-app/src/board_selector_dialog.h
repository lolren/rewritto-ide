#pragma once

#include <QDialog>
#include <QVector>

class QLineEdit;
class QModelIndex;
class QPushButton;
class QSortFilterProxyModel;
class QStandardItemModel;
class QTreeView;

class BoardSelectorDialog final : public QDialog {
  Q_OBJECT

 public:
  struct BoardEntry final {
    QString name;
    QString fqbn;
    bool isFavorite = false;
  };

  explicit BoardSelectorDialog(QWidget* parent = nullptr);

  void setBoards(QVector<BoardEntry> boards);
  void setCurrentFqbn(QString fqbn);
  QString selectedFqbn() const;

 signals:
  void favoriteToggled(QString fqbn);

 private:
  QString fqbnForProxyIndex(const QModelIndex& proxyIndex) const;

  QString currentFqbn_;
  QLineEdit* filterEdit_ = nullptr;
  QTreeView* table_ = nullptr;
  QStandardItemModel* model_ = nullptr;
  QSortFilterProxyModel* proxy_ = nullptr;
  QPushButton* selectButton_ = nullptr;
  QPushButton* cancelButton_ = nullptr;
};

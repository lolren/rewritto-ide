#pragma once

#include <QDialog>
#include <QVector>

class QLineEdit;
class QPushButton;
class QSortFilterProxyModel;
class QStandardItemModel;
class QTableView;

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
  QString currentFqbn_;
  QLineEdit* filterEdit_ = nullptr;
  QTableView* table_ = nullptr;
  QStandardItemModel* model_ = nullptr;
  QSortFilterProxyModel* proxy_ = nullptr;
  QPushButton* selectButton_ = nullptr;
  QPushButton* cancelButton_ = nullptr;
};


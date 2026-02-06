#pragma once

#include <QDialog>
#include <QIcon>
#include <QVariant>
#include <QVector>

class QLineEdit;
class QStandardItemModel;
class QSortFilterProxyModel;
class QTableView;

class QuickPickDialog final : public QDialog {
  Q_OBJECT

 public:
  struct Item final {
    QString label;
    QString detail;
    QVariant data;
    QIcon icon;
  };

  explicit QuickPickDialog(QWidget* parent = nullptr);

  void setPlaceholderText(QString placeholderText);
  void setItems(QVector<Item> items);
  QVariant selectedData() const;

 private:
  QLineEdit* filterEdit_ = nullptr;
  QTableView* table_ = nullptr;
  QStandardItemModel* model_ = nullptr;
  QSortFilterProxyModel* proxy_ = nullptr;
};

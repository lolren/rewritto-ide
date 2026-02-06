#include "quick_pick_dialog.h"

#include <QAbstractItemView>
#include <QBoxLayout>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>
#include <QtGlobal>

namespace {
constexpr int kColLabel = 0;
constexpr int kColDetail = 1;
constexpr int kColCount = 2;
constexpr int kRoleData = Qt::UserRole + 1;

class QuickPickFilterProxyModel final : public QSortFilterProxyModel {
 public:
  using QSortFilterProxyModel::QSortFilterProxyModel;

  void setQuery(QString query) {
    if (query_ == query) {
      return;
    }
    query_ = std::move(query);
    refreshFilter();
  }

 protected:
  bool filterAcceptsRow(int sourceRow,
                        const QModelIndex& sourceParent) const override {
    if (query_.trimmed().isEmpty()) {
      return true;
    }
    const QModelIndex labelIdx =
        sourceModel()->index(sourceRow, kColLabel, sourceParent);
    const QModelIndex detailIdx =
        sourceModel()->index(sourceRow, kColDetail, sourceParent);
    const QString label =
        sourceModel()->data(labelIdx, Qt::DisplayRole).toString();
    const QString detail =
        sourceModel()->data(detailIdx, Qt::DisplayRole).toString();

    const QString hay =
        (label + QStringLiteral(" ") + detail).toLower();

    const QStringList parts =
        query_.toLower().split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
      return true;
    }

    auto fuzzyContains = [](const QString& needle, const QString& s) -> bool {
      if (needle.isEmpty()) {
        return true;
      }
      int j = 0;
      for (int i = 0; i < s.size() && j < needle.size(); ++i) {
        if (s.at(i) == needle.at(j)) {
          ++j;
        }
      }
      return j == needle.size();
    };

    for (const QString& needle : parts) {
      if (!fuzzyContains(needle, hay)) {
        return false;
      }
    }
    return true;
  }

 private:
  void refreshFilter() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    endFilterChange();
#else
    invalidateFilter();
#endif
  }

  QString query_;
};

QString stripMnemonics(QString text) {
  text.remove(QLatin1Char('&'));
  return text.trimmed();
}
}  // namespace

QuickPickDialog::QuickPickDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(tr("Select"));
  resize(720, 520);

  filterEdit_ = new QLineEdit(this);
  filterEdit_->setPlaceholderText(tr("Type to filter\u2026"));

  model_ = new QStandardItemModel(0, kColCount, this);
  model_->setHorizontalHeaderLabels({tr("Name"), tr("Detail")});

  proxy_ = new QuickPickFilterProxyModel(this);
  proxy_->setSourceModel(model_);
  proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
  proxy_->setSortCaseSensitivity(Qt::CaseInsensitive);

  table_ = new QTableView(this);
  table_->setModel(proxy_);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->verticalHeader()->setVisible(false);
  table_->horizontalHeader()->setStretchLastSection(true);
  table_->setSortingEnabled(true);
  table_->sortByColumn(kColLabel, Qt::AscendingOrder);

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(filterEdit_);
  layout->addWidget(table_, 1);
  layout->addWidget(buttons);

  connect(filterEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
    if (!proxy_) {
      return;
    }
    static_cast<QuickPickFilterProxyModel*>(proxy_)->setQuery(text);
    if (proxy_->rowCount() > 0) {
      table_->setCurrentIndex(proxy_->index(0, 0));
      table_->scrollTo(proxy_->index(0, 0));
    }
  });

  auto updateOkEnabled = [this, buttons] {
    const QModelIndex current = table_ ? table_->currentIndex() : QModelIndex{};
    buttons->button(QDialogButtonBox::Ok)->setEnabled(current.isValid());
  };
  connect(table_->selectionModel(), &QItemSelectionModel::currentChanged, this,
          [updateOkEnabled](const QModelIndex&, const QModelIndex&) { updateOkEnabled(); });
  updateOkEnabled();

  connect(filterEdit_, &QLineEdit::returnPressed, this, [this] {
    if (table_ && table_->currentIndex().isValid()) {
      accept();
    }
  });
  connect(table_, &QTableView::doubleClicked, this, [this](const QModelIndex&) { accept(); });
}

void QuickPickDialog::setPlaceholderText(QString placeholderText) {
  if (filterEdit_) {
    filterEdit_->setPlaceholderText(stripMnemonics(std::move(placeholderText)));
  }
}

void QuickPickDialog::setItems(QVector<Item> items) {
  if (!model_) {
    return;
  }

  model_->removeRows(0, model_->rowCount());
  model_->setRowCount(0);

  for (const Item& item : items) {
    const QString label = stripMnemonics(item.label);
    if (label.isEmpty()) {
      continue;
    }
    auto* labelItem = new QStandardItem(label);
    labelItem->setData(item.data, kRoleData);
    if (!item.icon.isNull()) {
      labelItem->setIcon(item.icon);
    }
    auto* detailItem = new QStandardItem(stripMnemonics(item.detail));
    model_->appendRow({labelItem, detailItem});
  }

  table_->resizeColumnsToContents();
  if (proxy_ && proxy_->rowCount() > 0) {
    table_->setCurrentIndex(proxy_->index(0, 0));
    table_->scrollTo(proxy_->index(0, 0));
  }
  if (filterEdit_) {
    filterEdit_->selectAll();
    filterEdit_->setFocus();
  }
}

QVariant QuickPickDialog::selectedData() const {
  if (!table_ || !proxy_ || !model_) {
    return {};
  }
  const QModelIndex current = table_->currentIndex();
  if (!current.isValid()) {
    return {};
  }
  const QModelIndex src = proxy_->mapToSource(current);
  if (!src.isValid()) {
    return {};
  }
  const QModelIndex labelIdx = model_->index(src.row(), kColLabel);
  return model_->data(labelIdx, kRoleData);
}

#include "board_selector_dialog.h"

#include <algorithm>
#include <QAbstractItemView>
#include <QBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

namespace {
constexpr int kColFavorite = 0;
constexpr int kColName = 1;
constexpr int kColFqbn = 2;
constexpr int kColCount = 3;

constexpr int kRoleIsFavorite = Qt::UserRole + 1;

class StarDelegate final : public QStyledItemDelegate {
 public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter* painter,
             const QStyleOptionViewItem& option,
             const QModelIndex& index) const override {
    if (index.column() == kColFavorite) {
      const bool isFav = index.data(kRoleIsFavorite).toBool();
      painter->save();
      painter->setRenderHint(QPainter::Antialiasing);
      
      const QRect r = option.rect;
      const int size = 16;
      const QRect starRect(r.center().x() - size/2, r.center().y() - size/2, size, size);
      
      QPainterPath path;
      path.moveTo(8, 0);
      path.lineTo(10.5, 5);
      path.lineTo(16, 6);
      path.lineTo(12, 10);
      path.lineTo(13, 16);
      path.lineTo(8, 13);
      path.lineTo(3, 16);
      path.lineTo(4, 10);
      path.lineTo(0, 6);
      path.lineTo(5.5, 5);
      path.closeSubpath();
      
      painter->translate(starRect.topLeft());
      if (isFav) {
          painter->fillPath(path, QColor("#FFD700")); // Gold
          painter->drawPath(path);
      } else {
          painter->setPen(QPen(option.palette.color(QPalette::Text), 1));
          painter->drawPath(path);
      }
      painter->restore();
    } else {
      QStyledItemDelegate::paint(painter, option, index);
    }
  }

  QSize sizeHint(const QStyleOptionViewItem& option,
                const QModelIndex& index) const override {
    if (index.column() == kColFavorite) return QSize(32, 24);
    return QStyledItemDelegate::sizeHint(option, index);
  }
};

class BoardFilterProxyModel final : public QSortFilterProxyModel {
 public:
  using QSortFilterProxyModel::QSortFilterProxyModel;

  void setSelectedFqbn(const QString& fqbn) {
      selectedFqbn_ = fqbn;
      invalidate();
  }

 protected:
  bool filterAcceptsRow(int sourceRow,
                        const QModelIndex& sourceParent) const override {
    if (!sourceModel() || sourceRow < 0) {
      return false;
    }
    if (filterRegularExpression().pattern().isEmpty()) {
      return true;
    }
    const QModelIndex nameIdx = sourceModel()->index(sourceRow, kColName, sourceParent);
    const QModelIndex fqbnIdx = sourceModel()->index(sourceRow, kColFqbn, sourceParent);
    const QString name = sourceModel()->data(nameIdx, Qt::DisplayRole).toString();
    const QString fqbn = sourceModel()->data(fqbnIdx, Qt::DisplayRole).toString();
    return name.contains(filterRegularExpression()) ||
           fqbn.contains(filterRegularExpression());
  }

  bool lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const override {
      if (!sourceModel()) {
          return QSortFilterProxyModel::lessThan(source_left, source_right);
      }
      if (!source_left.isValid() || !source_right.isValid()) {
          return QSortFilterProxyModel::lessThan(source_left, source_right);
      }

      const QModelIndex fqbnLeftIndex = source_left.sibling(source_left.row(), kColFqbn);
      const QModelIndex fqbnRightIndex = source_right.sibling(source_right.row(), kColFqbn);
      const QString fqbnLeft = sourceModel()->data(fqbnLeftIndex, Qt::DisplayRole).toString();
      const QString fqbnRight = sourceModel()->data(fqbnRightIndex, Qt::DisplayRole).toString();

      // 1. Current selected board always on top
      if (!selectedFqbn_.isEmpty()) {
          bool isSelLeft = (fqbnLeft == selectedFqbn_);
          bool isSelRight = (fqbnRight == selectedFqbn_);
          if (isSelLeft != isSelRight) return isSelLeft;
      }

      const QModelIndex favLeftIndex = source_left.sibling(source_left.row(), kColFavorite);
      const QModelIndex favRightIndex = source_right.sibling(source_right.row(), kColFavorite);
      const bool favLeft = sourceModel()->data(favLeftIndex, kRoleIsFavorite).toBool();
      const bool favRight = sourceModel()->data(favRightIndex, kRoleIsFavorite).toBool();

      // 2. Favorites (bookmarks) under selected
      if (favLeft != favRight) {
          return favLeft;
      }

      return QSortFilterProxyModel::lessThan(source_left, source_right);
  }

 private:
  QString selectedFqbn_;
};
}  // namespace

BoardSelectorDialog::BoardSelectorDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(tr("Select Board"));
  resize(800, 520);

  filterEdit_ = new QLineEdit(this);
  filterEdit_->setPlaceholderText(tr("Search boards\u2026"));

  model_ = new QStandardItemModel(0, kColCount, this);
  model_->setHorizontalHeaderLabels({"", tr("Name"), tr("FQBN")});

  proxy_ = new BoardFilterProxyModel(this);
  proxy_->setSourceModel(model_);
  proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
  proxy_->setSortCaseSensitivity(Qt::CaseInsensitive);

  table_ = new QTableView(this);
  table_->setModel(proxy_);
  table_->setItemDelegate(new StarDelegate(this));
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->verticalHeader()->setVisible(false);
  table_->horizontalHeader()->setStretchLastSection(true);
  table_->horizontalHeader()->setSectionResizeMode(kColFavorite, QHeaderView::Fixed);
  table_->setColumnWidth(kColFavorite, 32);
  table_->setSortingEnabled(true);
  // Default sort by favorite (implicitly via lessThan) and name
  table_->sortByColumn(kColName, Qt::AscendingOrder);

  selectButton_ = new QPushButton(tr("Select"), this);
  selectButton_->setDefault(true);
  selectButton_->setEnabled(false);

  cancelButton_ = new QPushButton(tr("Cancel"), this);

  auto* buttons = new QHBoxLayout();
  buttons->addStretch(1);
  buttons->addWidget(selectButton_);
  buttons->addWidget(cancelButton_);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(filterEdit_);
  layout->addWidget(table_, 1);
  layout->addLayout(buttons);

  connect(cancelButton_, &QPushButton::clicked, this, &QDialog::reject);
  connect(selectButton_, &QPushButton::clicked, this, &QDialog::accept);

  connect(filterEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
    if (text.trimmed().isEmpty()) {
      proxy_->setFilterRegularExpression(QRegularExpression());
    } else {
      proxy_->setFilterRegularExpression(
          QRegularExpression(QRegularExpression::escape(text),
                             QRegularExpression::CaseInsensitiveOption));
    }
  });

  connect(table_, &QTableView::doubleClicked, this,
          [this](const QModelIndex& index) { 
              if (index.column() != kColFavorite) accept(); 
          });
  connect(table_->selectionModel(), &QItemSelectionModel::currentChanged, this,
          [this](const QModelIndex& current, const QModelIndex&) {
            selectButton_->setEnabled(current.isValid());
          });

  connect(table_, &QTableView::clicked, this, [this](const QModelIndex& index) {
      if (index.column() == kColFavorite) {
          const QModelIndex srcIdx = proxy_->mapToSource(index);
          if (!srcIdx.isValid()) {
              return;
          }
          const QString fqbn = model_->data(model_->index(srcIdx.row(), kColFqbn)).toString();
          if (fqbn.isEmpty()) {
              return;
          }
          bool isFav = !model_->data(srcIdx, kRoleIsFavorite).toBool();
          model_->setData(srcIdx, isFav, kRoleIsFavorite);
          emit favoriteToggled(fqbn);
          proxy_->invalidate(); // Trigger re-sort
          proxy_->sort(kColName, Qt::AscendingOrder);
      }
  });
}

void BoardSelectorDialog::setBoards(QVector<BoardEntry> boards) {
  model_->removeRows(0, model_->rowCount());
  model_->setRowCount(0);

  for (const auto& b : boards) {
    if (b.fqbn.trimmed().isEmpty()) {
      continue;
    }
    QList<QStandardItem*> row;
    auto* favItem = new QStandardItem();
    favItem->setData(b.isFavorite, kRoleIsFavorite);
    row << favItem
        << new QStandardItem(b.name.trimmed().isEmpty() ? b.fqbn : b.name)
        << new QStandardItem(b.fqbn);
    model_->appendRow(row);
  }

  proxy_->invalidate();
  proxy_->sort(kColName, Qt::AscendingOrder);
  table_->resizeColumnToContents(kColName);
}

void BoardSelectorDialog::setCurrentFqbn(QString fqbn) {
  currentFqbn_ = fqbn.trimmed();
  if (proxy_) {
      auto* boardProxy = static_cast<BoardFilterProxyModel*>(proxy_);
      boardProxy->setSelectedFqbn(currentFqbn_);
  }
  
  if (currentFqbn_.isEmpty()) {
    return;
  }

  const int rows = model_->rowCount();
  for (int r = 0; r < rows; ++r) {
    const QModelIndex src = model_->index(r, kColFqbn);
    if (model_->data(src, Qt::DisplayRole).toString().trimmed() != currentFqbn_) {
      continue;
    }
    const QModelIndex proxyIdx = proxy_->mapFromSource(src);
    if (!proxyIdx.isValid()) {
      continue;
    }
    table_->setCurrentIndex(proxyIdx);
    table_->scrollTo(proxyIdx, QAbstractItemView::PositionAtCenter);
    return;
  }
}

QString BoardSelectorDialog::selectedFqbn() const {
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
  const QModelIndex fqbnIdx = model_->index(src.row(), kColFqbn);
  return model_->data(fqbnIdx, Qt::DisplayRole).toString().trimmed();
}

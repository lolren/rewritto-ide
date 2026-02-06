#include "board_selector_dialog.h"

#include <algorithm>
#include <QAbstractItemView>
#include <QBoxLayout>
#include <QFont>
#include <QHeaderView>
#include <QIcon>
#include <QLineEdit>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTreeView>

namespace {
constexpr int kColName = 0;
constexpr int kColFqbn = 1;
constexpr int kColFavorite = 2;
constexpr int kColCount = 3;

constexpr int kRoleIsFavorite = Qt::UserRole + 1;
constexpr int kRoleNodeType = Qt::UserRole + 2;
constexpr int kRoleCoreId = Qt::UserRole + 3;
constexpr int kRoleCoreColor = Qt::UserRole + 4;

enum class NodeType : int { Core = 0, Board = 1 };

QString coreIdFromFqbn(QString fqbn) {
  const QStringList parts = fqbn.trimmed().split(QLatin1Char(':'));
  if (parts.size() >= 2) {
    const QString vendor = parts.at(0).trimmed();
    const QString arch = parts.at(1).trimmed();
    if (!vendor.isEmpty() && !arch.isEmpty()) {
      return QStringLiteral("%1:%2").arg(vendor, arch);
    }
  }
  return fqbn.trimmed();
}

QString coreLabel(QString coreId) {
  coreId = coreId.trimmed();
  const QStringList parts = coreId.split(QLatin1Char(':'));
  if (parts.size() >= 2) {
    const QString vendor = parts.at(0).trimmed();
    const QString arch = parts.at(1).trimmed();
    if (!vendor.isEmpty() && !arch.isEmpty()) {
      return QStringLiteral("%1 core (%2)").arg(vendor, arch);
    }
  }
  return coreId;
}

QColor coreColorFor(const QString& coreId, const QPalette& palette) {
  const bool darkBase = palette.base().color().lightness() < 128;
  const uint hash = qHash(coreId);
  const int hue = static_cast<int>(hash % 360U);
  const int saturation = darkBase ? 160 : 185;
  int lightness = darkBase ? 175 : 92;
  if (hue >= 45 && hue <= 80) {
    lightness = darkBase ? 200 : 70;
  }
  QColor color = QColor::fromHsl(hue, saturation, lightness);
  color = darkBase ? color.lighter(112) : color.darker(105);
  return color;
}

QIcon coreDotIcon(const QColor& color, const QPalette& palette) {
  QPixmap pixmap(10, 10);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(color);
  painter.setPen(QPen(palette.base().color(), 1));
  painter.drawEllipse(QRectF(1, 1, 8, 8));
  return QIcon(pixmap);
}

class StarDelegate final : public QStyledItemDelegate {
 public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter* painter,
             const QStyleOptionViewItem& option,
             const QModelIndex& index) const override {
    if (index.column() == kColFavorite &&
        index.data(kRoleNodeType).toInt() == static_cast<int>(NodeType::Board)) {
      const bool isFav = index.data(kRoleIsFavorite).toBool();
      painter->save();
      painter->setRenderHint(QPainter::Antialiasing);

      const QRect r = option.rect;
      const int size = 16;
      const QRect starRect(r.center().x() - size / 2, r.center().y() - size / 2, size,
                           size);

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
        painter->fillPath(path, QColor("#FFD700"));
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
    selectedFqbn_ = fqbn.trimmed();
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
    const QModelIndex nameIdx =
        sourceModel()->index(sourceRow, kColName, sourceParent);
    const QModelIndex fqbnIdx =
        sourceModel()->index(sourceRow, kColFqbn, sourceParent);
    if (!nameIdx.isValid()) {
      return false;
    }
    const QString name = sourceModel()->data(nameIdx, Qt::DisplayRole).toString();
    const QString fqbn = sourceModel()->data(fqbnIdx, Qt::DisplayRole).toString();
    const QString coreId = sourceModel()->data(nameIdx, kRoleCoreId).toString();
    if (name.contains(filterRegularExpression()) ||
        fqbn.contains(filterRegularExpression()) ||
        coreId.contains(filterRegularExpression())) {
      return true;
    }
    const int childRows = sourceModel()->rowCount(nameIdx);
    for (int child = 0; child < childRows; ++child) {
      if (filterAcceptsRow(child, nameIdx)) {
        return true;
      }
    }
    return false;
  }

  bool lessThan(const QModelIndex& sourceLeft,
                const QModelIndex& sourceRight) const override {
    if (!sourceModel() || !sourceLeft.isValid() || !sourceRight.isValid()) {
      return QSortFilterProxyModel::lessThan(sourceLeft, sourceRight);
    }

    const QModelIndex nameLeft = sourceLeft.sibling(sourceLeft.row(), kColName);
    const QModelIndex nameRight =
        sourceRight.sibling(sourceRight.row(), kColName);
    const NodeType nodeLeft = static_cast<NodeType>(
        sourceModel()->data(nameLeft, kRoleNodeType).toInt());
    const NodeType nodeRight = static_cast<NodeType>(
        sourceModel()->data(nameRight, kRoleNodeType).toInt());
    if (nodeLeft == NodeType::Board && nodeRight == NodeType::Board) {
      const QModelIndex fqbnLeftIndex =
          sourceLeft.sibling(sourceLeft.row(), kColFqbn);
      const QModelIndex fqbnRightIndex =
          sourceRight.sibling(sourceRight.row(), kColFqbn);
      const QString fqbnLeft =
          sourceModel()->data(fqbnLeftIndex, Qt::DisplayRole).toString().trimmed();
      const QString fqbnRight =
          sourceModel()->data(fqbnRightIndex, Qt::DisplayRole).toString().trimmed();
      if (!selectedFqbn_.isEmpty()) {
        const bool isSelLeft = (fqbnLeft == selectedFqbn_);
        const bool isSelRight = (fqbnRight == selectedFqbn_);
        if (isSelLeft != isSelRight) {
          return isSelLeft;
        }
      }

      const QModelIndex favLeftIndex =
          sourceLeft.sibling(sourceLeft.row(), kColFavorite);
      const QModelIndex favRightIndex =
          sourceRight.sibling(sourceRight.row(), kColFavorite);
      const bool favLeft =
          sourceModel()->data(favLeftIndex, kRoleIsFavorite).toBool();
      const bool favRight =
          sourceModel()->data(favRightIndex, kRoleIsFavorite).toBool();
      if (favLeft != favRight) {
        return favLeft;
      }
    }

    const QString leftText = sourceModel()->data(nameLeft, Qt::DisplayRole).toString();
    const QString rightText =
        sourceModel()->data(nameRight, Qt::DisplayRole).toString();
    return QString::localeAwareCompare(leftText, rightText) < 0;
  }

 private:
  QString selectedFqbn_;
};
}  // namespace

BoardSelectorDialog::BoardSelectorDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(tr("Select Board"));
  resize(800, 520);

  filterEdit_ = new QLineEdit(this);
  filterEdit_->setPlaceholderText(tr("Search cores or boards…"));

  model_ = new QStandardItemModel(0, kColCount, this);
  model_->setHorizontalHeaderLabels({tr("Board / Core"), tr("FQBN"), ""});

  proxy_ = new BoardFilterProxyModel(this);
  proxy_->setSourceModel(model_);
  proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
  proxy_->setSortCaseSensitivity(Qt::CaseInsensitive);
  proxy_->setDynamicSortFilter(true);
  proxy_->setRecursiveFilteringEnabled(true);
  proxy_->setAutoAcceptChildRows(true);

  table_ = new QTreeView(this);
  table_->setModel(proxy_);
  table_->setItemDelegateForColumn(kColFavorite, new StarDelegate(this));
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->setRootIsDecorated(true);
  table_->setItemsExpandable(true);
  table_->setUniformRowHeights(true);
  table_->setAllColumnsShowFocus(true);
  table_->setAlternatingRowColors(true);
  table_->header()->setStretchLastSection(false);
  table_->header()->setSectionResizeMode(kColName, QHeaderView::Stretch);
  table_->header()->setSectionResizeMode(kColFqbn, QHeaderView::Interactive);
  table_->header()->setSectionResizeMode(kColFavorite, QHeaderView::Fixed);
  table_->setColumnWidth(kColFavorite, 34);
  table_->setSortingEnabled(true);
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
      table_->collapseAll();
      setCurrentFqbn(currentFqbn_);
    } else {
      proxy_->setFilterRegularExpression(
          QRegularExpression(QRegularExpression::escape(text),
                             QRegularExpression::CaseInsensitiveOption));
      table_->expandAll();
    }
  });

  connect(table_, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
    if (index.column() == kColFavorite) {
      return;
    }
    if (!fqbnForProxyIndex(index).isEmpty()) {
      accept();
    }
  });
  connect(table_->selectionModel(), &QItemSelectionModel::currentChanged, this,
          [this](const QModelIndex& current, const QModelIndex&) {
            selectButton_->setEnabled(!fqbnForProxyIndex(current).isEmpty());
          });

  connect(table_, &QTreeView::clicked, this, [this](const QModelIndex& index) {
    if (index.column() != kColFavorite) {
      return;
    }
    const QModelIndex srcIdx = proxy_->mapToSource(index);
    if (!srcIdx.isValid()) {
      return;
    }
    if (model_->data(srcIdx, kRoleNodeType).toInt() !=
        static_cast<int>(NodeType::Board)) {
      return;
    }
    const QModelIndex fqbnIdx = model_->index(srcIdx.row(), kColFqbn, srcIdx.parent());
    const QString fqbn =
        model_->data(fqbnIdx, Qt::DisplayRole).toString().trimmed();
    if (fqbn.isEmpty()) {
      return;
    }
    const bool isFav = !model_->data(srcIdx, kRoleIsFavorite).toBool();
    model_->setData(srcIdx, isFav, kRoleIsFavorite);
    emit favoriteToggled(fqbn);
    proxy_->invalidate();
    proxy_->sort(kColName, Qt::AscendingOrder);
    const QModelIndex refreshed = proxy_->mapFromSource(srcIdx);
    if (refreshed.isValid()) {
      table_->setCurrentIndex(refreshed.sibling(refreshed.row(), kColName));
    }
  });
}

void BoardSelectorDialog::setBoards(QVector<BoardEntry> boards) {
  model_->removeRows(0, model_->rowCount());
  model_->setRowCount(0);

  QMap<QString, QVector<BoardEntry>> boardsByCore;
  for (const auto& b : boards) {
    const QString fqbn = b.fqbn.trimmed();
    if (fqbn.isEmpty()) {
      continue;
    }
    BoardEntry entry = b;
    entry.fqbn = fqbn;
    entry.name = b.name.trimmed().isEmpty() ? fqbn : b.name.trimmed();
    boardsByCore[coreIdFromFqbn(fqbn)].append(entry);
  }

  const QPalette pal = palette();
  for (auto it = boardsByCore.begin(); it != boardsByCore.end(); ++it) {
    QVector<BoardEntry> coreBoards = it.value();
    std::sort(coreBoards.begin(), coreBoards.end(),
              [](const BoardEntry& left, const BoardEntry& right) {
                return QString::localeAwareCompare(left.name, right.name) < 0;
              });

    const QString coreId = it.key();
    const QColor coreColor = coreColorFor(coreId, pal);
    const QIcon coreIcon = coreDotIcon(coreColor, pal);

    auto* coreName = new QStandardItem(coreLabel(coreId));
    coreName->setData(static_cast<int>(NodeType::Core), kRoleNodeType);
    coreName->setData(coreId, kRoleCoreId);
    coreName->setData(coreColor, kRoleCoreColor);
    coreName->setData(coreIcon, Qt::DecorationRole);
    coreName->setData(coreColor, Qt::ForegroundRole);
    coreName->setEditable(false);
    coreName->setSelectable(false);
    QFont coreFont = coreName->font();
    coreFont.setBold(true);
    coreName->setFont(coreFont);

    auto* coreFqbn = new QStandardItem(tr("%1 board(s)").arg(coreBoards.size()));
    coreFqbn->setData(static_cast<int>(NodeType::Core), kRoleNodeType);
    coreFqbn->setData(coreId, kRoleCoreId);
    coreFqbn->setEditable(false);
    coreFqbn->setSelectable(false);

    auto* coreFav = new QStandardItem();
    coreFav->setData(static_cast<int>(NodeType::Core), kRoleNodeType);
    coreFav->setEditable(false);
    coreFav->setSelectable(false);

    model_->appendRow({coreName, coreFqbn, coreFav});

    for (const auto& board : coreBoards) {
      auto* nameItem = new QStandardItem(board.name);
      nameItem->setData(static_cast<int>(NodeType::Board), kRoleNodeType);
      nameItem->setData(coreId, kRoleCoreId);
      nameItem->setData(coreColor, kRoleCoreColor);
      nameItem->setData(coreIcon, Qt::DecorationRole);

      auto* fqbnItem = new QStandardItem(board.fqbn);
      fqbnItem->setData(static_cast<int>(NodeType::Board), kRoleNodeType);
      fqbnItem->setData(coreId, kRoleCoreId);
      fqbnItem->setData(coreColor, kRoleCoreColor);

      auto* favItem = new QStandardItem();
      favItem->setData(board.isFavorite, kRoleIsFavorite);
      favItem->setData(static_cast<int>(NodeType::Board), kRoleNodeType);
      favItem->setData(coreId, kRoleCoreId);
      favItem->setData(coreColor, kRoleCoreColor);

      coreName->appendRow({nameItem, fqbnItem, favItem});
    }
  }

  proxy_->invalidate();
  proxy_->sort(kColName, Qt::AscendingOrder);
  table_->collapseAll();
  table_->resizeColumnToContents(kColFqbn);
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

  const QList<QStandardItem*> matches =
      model_->findItems(currentFqbn_, Qt::MatchExactly | Qt::MatchRecursive,
                        kColFqbn);
  if (matches.isEmpty()) {
    return;
  }

  const QModelIndex src = matches.first()->index();
  const QModelIndex proxyIdx = proxy_->mapFromSource(src);
  if (!proxyIdx.isValid()) {
    return;
  }

  QModelIndex parent = proxyIdx.parent();
  while (parent.isValid()) {
    table_->expand(parent);
    parent = parent.parent();
  }
  const QModelIndex nameIndex = proxyIdx.sibling(proxyIdx.row(), kColName);
  table_->setCurrentIndex(nameIndex);
  table_->scrollTo(nameIndex, QAbstractItemView::PositionAtCenter);
}

QString BoardSelectorDialog::selectedFqbn() const {
  if (!table_ || !proxy_ || !model_) {
    return {};
  }
  return fqbnForProxyIndex(table_->currentIndex());
}

QString BoardSelectorDialog::fqbnForProxyIndex(
    const QModelIndex& proxyIndex) const {
  if (!proxy_ || !model_ || !proxyIndex.isValid()) {
    return {};
  }

  const QModelIndex src = proxy_->mapToSource(proxyIndex);
  if (!src.isValid()) {
    return {};
  }

  const QModelIndex srcName = src.sibling(src.row(), kColName);
  if (model_->data(srcName, kRoleNodeType).toInt() !=
      static_cast<int>(NodeType::Board)) {
    return {};
  }
  const QModelIndex fqbnIdx = src.sibling(src.row(), kColFqbn);
  return model_->data(fqbnIdx, Qt::DisplayRole).toString().trimmed();
}

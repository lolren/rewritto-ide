#include "code_snapshots_dialog.h"

#include "code_snapshot_store.h"

#include <QAbstractItemView>
#include <QBoxLayout>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>

namespace {
constexpr int kColWhen = 0;
constexpr int kColComment = 1;
constexpr int kColSize = 2;
constexpr int kColCount = 3;

constexpr int kRoleSnapshotId = Qt::UserRole + 1;
constexpr int kRoleSnapshotComment = Qt::UserRole + 2;

class SnapshotsFilterProxyModel final : public QSortFilterProxyModel {
 public:
  using QSortFilterProxyModel::QSortFilterProxyModel;

  void setQuery(QString query) {
    if (query_ == query) {
      return;
    }
    beginFilterChange();
    query_ = std::move(query);
    endFilterChange();
  }

 protected:
  bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
    if (query_.trimmed().isEmpty()) {
      return true;
    }
    const QModelIndex whenIdx = sourceModel()->index(sourceRow, kColWhen, sourceParent);
    const QModelIndex commentIdx = sourceModel()->index(sourceRow, kColComment, sourceParent);
    const QModelIndex sizeIdx = sourceModel()->index(sourceRow, kColSize, sourceParent);

    const QString when = sourceModel()->data(whenIdx, Qt::DisplayRole).toString();
    const QString comment = sourceModel()->data(commentIdx, Qt::DisplayRole).toString();
    const QString size = sourceModel()->data(sizeIdx, Qt::DisplayRole).toString();
    const QString id = sourceModel()->data(whenIdx, kRoleSnapshotId).toString();

    const QString hay = (when + QStringLiteral(" ") + comment + QStringLiteral(" ") + size +
                         QStringLiteral(" ") + id)
                            .toLower();

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
  QString query_;
};

QString formatBytes(qint64 bytes) {
  const double b = static_cast<double>(bytes);
  if (b < 1024.0) {
    return QStringLiteral("%1 B").arg(bytes);
  }
  if (b < 1024.0 * 1024.0) {
    return QStringLiteral("%1 KB").arg(b / 1024.0, 0, 'f', 1);
  }
  return QStringLiteral("%1 MB").arg(b / (1024.0 * 1024.0), 0, 'f', 1);
}
}  // namespace

CodeSnapshotsDialog::CodeSnapshotsDialog(QString sketchFolder, QWidget* parent)
    : QDialog(parent), sketchFolder_(std::move(sketchFolder)) {
  setWindowTitle(tr("Code Snapshots"));
  resize(760, 520);

  filterEdit_ = new QLineEdit(this);
  filterEdit_->setPlaceholderText(tr("Type to filter\u2026"));

  model_ = new QStandardItemModel(0, kColCount, this);
  model_->setHorizontalHeaderLabels({tr("When"), tr("Comment"), tr("Size")});

  proxy_ = new SnapshotsFilterProxyModel(this);
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
  table_->setSortingEnabled(false);

  auto* buttons = new QDialogButtonBox(this);
  auto* newBtn = buttons->addButton(tr("New Snapshot\u2026"), QDialogButtonBox::ActionRole);
  restoreButton_ = buttons->addButton(tr("Restore"), QDialogButtonBox::AcceptRole);
  editCommentButton_ = buttons->addButton(tr("Edit Comment\u2026"), QDialogButtonBox::ActionRole);
  deleteButton_ = buttons->addButton(tr("Delete"), QDialogButtonBox::DestructiveRole);
  buttons->addButton(QDialogButtonBox::Close);

  restoreButton_->setEnabled(false);
  editCommentButton_->setEnabled(false);
  deleteButton_->setEnabled(false);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(filterEdit_);
  layout->addWidget(table_, 1);
  layout->addWidget(buttons);

  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(newBtn, &QPushButton::clicked, this, [this] { emit captureRequested(); });
  connect(restoreButton_, &QPushButton::clicked, this, [this] {
    const QString id = selectedSnapshotId();
    if (!id.isEmpty()) {
      emit restoreRequested(id);
    }
  });
  connect(editCommentButton_, &QPushButton::clicked, this, [this] {
    const QString id = selectedSnapshotId();
    if (id.isEmpty()) {
      return;
    }
    const QModelIndex current = table_ ? table_->currentIndex() : QModelIndex{};
    if (!current.isValid() || !proxy_ || !model_) {
      return;
    }
    const QModelIndex src = proxy_->mapToSource(current);
    const QModelIndex whenIdx = model_->index(src.row(), kColWhen);
    const QString comment = model_->data(whenIdx, kRoleSnapshotComment).toString();
    emit editCommentRequested(id, comment);
  });
  connect(deleteButton_, &QPushButton::clicked, this, [this] {
    const QString id = selectedSnapshotId();
    if (!id.isEmpty()) {
      emit deleteRequested(id);
    }
  });

  connect(filterEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
    if (!proxy_) {
      return;
    }
    static_cast<SnapshotsFilterProxyModel*>(proxy_)->setQuery(text);
    if (proxy_->rowCount() > 0 && table_) {
      table_->setCurrentIndex(proxy_->index(0, 0));
      table_->scrollTo(proxy_->index(0, 0));
    }
    updateSelectionActions();
  });

  if (table_ && table_->selectionModel()) {
    connect(table_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex&, const QModelIndex&) { updateSelectionActions(); });
  }
  connect(table_, &QTableView::doubleClicked, this, [this](const QModelIndex&) {
    const QString id = selectedSnapshotId();
    if (!id.isEmpty()) {
      emit restoreRequested(id);
    }
  });

  reload();
}

void CodeSnapshotsDialog::reload() {
  if (!model_ || sketchFolder_.trimmed().isEmpty()) {
    return;
  }

  model_->removeRows(0, model_->rowCount());
  model_->setRowCount(0);

  QString err;
  const QVector<CodeSnapshotStore::SnapshotMeta> snapshots =
      CodeSnapshotStore::listSnapshots(sketchFolder_, &err);

  QLocale locale;
  for (const auto& meta : snapshots) {
    auto* whenItem = new QStandardItem(locale.toString(meta.createdAtUtc.toLocalTime(),
                                                      QLocale::ShortFormat));
    whenItem->setData(meta.id, kRoleSnapshotId);
    whenItem->setData(meta.comment, kRoleSnapshotComment);

    auto* commentItem = new QStandardItem(meta.comment);
    const QString sizeText =
        tr("%1 files, %2").arg(meta.fileCount).arg(formatBytes(meta.totalBytes));
    auto* sizeItem = new QStandardItem(sizeText);

    model_->appendRow({whenItem, commentItem, sizeItem});
  }

  if (table_) {
    table_->resizeColumnsToContents();
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(kColComment, QHeaderView::Stretch);
  }

  if (proxy_ && proxy_->rowCount() > 0 && table_) {
    table_->setCurrentIndex(proxy_->index(0, 0));
    table_->scrollTo(proxy_->index(0, 0));
  }
  updateSelectionActions();
}

QString CodeSnapshotsDialog::selectedSnapshotId() const {
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
  const QModelIndex whenIdx = model_->index(src.row(), kColWhen);
  return model_->data(whenIdx, kRoleSnapshotId).toString().trimmed();
}

void CodeSnapshotsDialog::updateSelectionActions() {
  const bool hasSelection = !selectedSnapshotId().isEmpty();
  if (restoreButton_) {
    restoreButton_->setEnabled(hasSelection);
  }
  if (editCommentButton_) {
    editCommentButton_->setEnabled(hasSelection);
  }
  if (deleteButton_) {
    deleteButton_->setEnabled(hasSelection);
  }
}


#include "code_snapshot_compare_dialog.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVariant>
#include <QVBoxLayout>

namespace {
const QString kCurrentSourceId = QStringLiteral("__current__");

QString normalizeRelativePath(QString rel) {
  rel = rel.trimmed();
  rel.replace('\\', '/');
  rel = QDir::cleanPath(rel);
  if (rel == QStringLiteral(".")) {
    return {};
  }
  while (rel.startsWith(QStringLiteral("./"))) {
    rel = rel.mid(2);
  }
  return rel;
}

QString statusForEntry(bool leftExists, bool rightExists, bool unchanged, const QObject* owner) {
  if (unchanged) {
    return owner->tr("Unchanged");
  }
  if (leftExists && !rightExists) {
    return owner->tr("Removed");
  }
  if (!leftExists && rightExists) {
    return owner->tr("Added");
  }
  return owner->tr("Modified");
}

QString formatDeltaBytes(int deltaBytes) {
  if (deltaBytes > 0) {
    return QStringLiteral("+%1 B").arg(deltaBytes);
  }
  if (deltaBytes < 0) {
    return QStringLiteral("%1 B").arg(deltaBytes);
  }
  return QStringLiteral("0 B");
}

QString decodeFileText(const QByteArray& bytes, const QObject* owner) {
  if (bytes.contains('\0')) {
    return owner->tr("[Binary file: %1 bytes]").arg(bytes.size());
  }
  return QString::fromUtf8(bytes);
}

QString snapshotDisplayLabel(const CodeSnapshotStore::SnapshotMeta& meta, const QObject* owner) {
  const QString when =
      QLocale().toString(meta.createdAtUtc.toLocalTime(), QLocale::ShortFormat);
  const QString comment = meta.comment.trimmed();
  if (!comment.isEmpty()) {
    return owner->tr("%1  %2").arg(when, comment);
  }
  return owner->tr("%1  (%2)").arg(when, meta.id.left(12));
}
}  // namespace

CodeSnapshotCompareDialog::CodeSnapshotCompareDialog(
    QString sketchFolder,
    QHash<QString, QByteArray> currentFiles,
    QWidget* parent)
    : QDialog(parent),
      sketchFolder_(std::move(sketchFolder)),
      currentFiles_(std::move(currentFiles)) {
  setWindowTitle(tr("Compare Code Snapshots"));
  resize(1080, 700);

  leftCombo_ = new QComboBox(this);
  rightCombo_ = new QComboBox(this);
  showUnchangedCheck_ = new QCheckBox(tr("Show unchanged files"), this);
  summaryLabel_ = new QLabel(this);
  summaryLabel_->setWordWrap(true);

  auto* leftSourceLabel = new QLabel(tr("Left:"), this);
  auto* rightSourceLabel = new QLabel(tr("Right:"), this);
  auto* sourceRow = new QHBoxLayout();
  sourceRow->addWidget(leftSourceLabel);
  sourceRow->addWidget(leftCombo_, 1);
  sourceRow->addSpacing(12);
  sourceRow->addWidget(rightSourceLabel);
  sourceRow->addWidget(rightCombo_, 1);
  sourceRow->addSpacing(8);
  sourceRow->addWidget(showUnchangedCheck_);

  filesTable_ = new QTableWidget(this);
  filesTable_->setColumnCount(3);
  filesTable_->setHorizontalHeaderLabels({tr("File"), tr("Status"), tr("Delta")});
  filesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
  filesTable_->setSelectionMode(QAbstractItemView::SingleSelection);
  filesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  filesTable_->setAlternatingRowColors(true);
  filesTable_->verticalHeader()->setVisible(false);
  filesTable_->horizontalHeader()->setStretchLastSection(false);
  filesTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  filesTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  filesTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

  leftLabel_ = new QLabel(tr("Left"), this);
  rightLabel_ = new QLabel(tr("Right"), this);
  leftPreview_ = new QPlainTextEdit(this);
  rightPreview_ = new QPlainTextEdit(this);
  leftPreview_->setReadOnly(true);
  rightPreview_->setReadOnly(true);

  const QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  leftPreview_->setFont(monoFont);
  rightPreview_->setFont(monoFont);

  auto* leftPreviewLayout = new QVBoxLayout();
  leftPreviewLayout->setContentsMargins(0, 0, 0, 0);
  leftPreviewLayout->addWidget(leftLabel_);
  leftPreviewLayout->addWidget(leftPreview_, 1);
  auto* leftPreviewContainer = new QWidget(this);
  leftPreviewContainer->setLayout(leftPreviewLayout);

  auto* rightPreviewLayout = new QVBoxLayout();
  rightPreviewLayout->setContentsMargins(0, 0, 0, 0);
  rightPreviewLayout->addWidget(rightLabel_);
  rightPreviewLayout->addWidget(rightPreview_, 1);
  auto* rightPreviewContainer = new QWidget(this);
  rightPreviewContainer->setLayout(rightPreviewLayout);

  auto* previewSplitter = new QSplitter(Qt::Horizontal, this);
  previewSplitter->addWidget(leftPreviewContainer);
  previewSplitter->addWidget(rightPreviewContainer);
  previewSplitter->setStretchFactor(0, 1);
  previewSplitter->setStretchFactor(1, 1);

  auto* bodySplitter = new QSplitter(Qt::Vertical, this);
  bodySplitter->addWidget(filesTable_);
  bodySplitter->addWidget(previewSplitter);
  bodySplitter->setStretchFactor(0, 1);
  bodySplitter->setStretchFactor(1, 2);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto* layout = new QVBoxLayout(this);
  layout->addLayout(sourceRow);
  layout->addWidget(summaryLabel_);
  layout->addWidget(bodySplitter, 1);
  layout->addWidget(buttons);

  populateSourceCombos();

  connect(leftCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { rebuildComparison(); });
  connect(rightCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { rebuildComparison(); });
  connect(showUnchangedCheck_, &QCheckBox::toggled, this,
          [this](bool) { rebuildComparison(); });
  connect(filesTable_, &QTableWidget::itemSelectionChanged, this,
          [this] { updatePreview(); });

  rebuildComparison();
}

void CodeSnapshotCompareDialog::populateSourceCombos() {
  snapshots_ = CodeSnapshotStore::listSnapshots(sketchFolder_);

  leftCombo_->clear();
  rightCombo_->clear();

  rightCombo_->addItem(tr("Current Code (Workspace)"), kCurrentSourceId);

  for (const auto& snapshot : snapshots_) {
    const QString label = snapshotDisplayLabel(snapshot, this);
    leftCombo_->addItem(label, snapshot.id);
    rightCombo_->addItem(label, snapshot.id);
  }

  if (leftCombo_->count() > 0) {
    leftCombo_->setCurrentIndex(0);
  }

  if (rightCombo_->count() > 1) {
    const QString firstSnapshotId = leftCombo_->currentData().toString();
    int matchInRight = rightCombo_->findData(firstSnapshotId);
    if (matchInRight > 1 && rightCombo_->count() > 2) {
      matchInRight = 2;
    } else {
      matchInRight = 0;
    }
    rightCombo_->setCurrentIndex(std::max(0, matchInRight));
  } else {
    rightCombo_->setCurrentIndex(0);
  }
}

bool CodeSnapshotCompareDialog::loadSnapshotFiles(const QString& snapshotId,
                                                  QString* outError) {
  const QString id = snapshotId.trimmed();
  if (id.isEmpty()) {
    if (outError) {
      *outError = tr("Snapshot id is missing.");
    }
    return false;
  }
  if (snapshotFilesById_.contains(id)) {
    return true;
  }

  QString err;
  const auto snapshot = CodeSnapshotStore::readSnapshot(sketchFolder_, id, &err);
  if (!snapshot) {
    if (outError) {
      *outError = err.isEmpty() ? tr("Failed to read snapshot.") : err;
    }
    return false;
  }

  const QString filesRootPath =
      QDir(CodeSnapshotStore::snapshotsRootForSketch(sketchFolder_))
          .filePath(id + QStringLiteral("/files"));
  QDir filesRoot(filesRootPath);
  if (!filesRoot.exists()) {
    if (outError) {
      *outError = tr("Snapshot files are missing.");
    }
    return false;
  }

  QHash<QString, QByteArray> files;
  files.reserve(snapshot->files.size());
  for (const auto& fileMeta : snapshot->files) {
    const QString rel = normalizeRelativePath(fileMeta.relativePath);
    if (rel.isEmpty()) {
      continue;
    }

    QFile file(filesRoot.filePath(rel));
    if (!file.open(QIODevice::ReadOnly)) {
      if (outError) {
        *outError = tr("Failed to read snapshot file '%1'.").arg(rel);
      }
      return false;
    }
    files.insert(rel, file.readAll());
  }

  snapshotFilesById_.insert(id, std::move(files));
  return true;
}

const QHash<QString, QByteArray>* CodeSnapshotCompareDialog::filesForSourceId(
    const QString& sourceId,
    QString* outError) {
  const QString id = sourceId.trimmed();
  if (id == kCurrentSourceId) {
    return &currentFiles_;
  }
  if (!loadSnapshotFiles(id, outError)) {
    return nullptr;
  }
  return &snapshotFilesById_[id];
}

QString CodeSnapshotCompareDialog::displayLabelForSourceId(const QString& sourceId) const {
  const QString id = sourceId.trimmed();
  if (id == kCurrentSourceId) {
    return tr("Current Code (Workspace)");
  }
  for (const auto& snapshot : snapshots_) {
    if (snapshot.id == id) {
      return snapshotDisplayLabel(snapshot, this);
    }
  }
  return id;
}

void CodeSnapshotCompareDialog::rebuildComparison() {
  visibleEntries_.clear();
  filesTable_->clearContents();
  filesTable_->setRowCount(0);
  leftPreview_->clear();
  rightPreview_->clear();

  const QString leftId = leftCombo_->currentData().toString().trimmed();
  const QString rightId = rightCombo_->currentData().toString().trimmed();
  if (leftId.isEmpty() || rightId.isEmpty()) {
    summaryLabel_->setText(tr("No sources available to compare."));
    return;
  }

  QString err;
  const QHash<QString, QByteArray>* leftFiles = filesForSourceId(leftId, &err);
  if (!leftFiles) {
    summaryLabel_->setText(err.isEmpty() ? tr("Failed to load left source.") : err);
    return;
  }
  const QHash<QString, QByteArray>* rightFiles = filesForSourceId(rightId, &err);
  if (!rightFiles) {
    summaryLabel_->setText(err.isEmpty() ? tr("Failed to load right source.") : err);
    return;
  }

  QSet<QString> pathSet;
  for (auto it = leftFiles->constBegin(); it != leftFiles->constEnd(); ++it) {
    pathSet.insert(it.key());
  }
  for (auto it = rightFiles->constBegin(); it != rightFiles->constEnd(); ++it) {
    pathSet.insert(it.key());
  }

  QStringList paths = pathSet.values();
  std::sort(paths.begin(), paths.end());

  int addedCount = 0;
  int removedCount = 0;
  int modifiedCount = 0;
  int unchangedCount = 0;
  const bool showUnchanged = showUnchangedCheck_->isChecked();

  for (const QString& path : paths) {
    const bool leftExists = leftFiles->contains(path);
    const bool rightExists = rightFiles->contains(path);
    const QByteArray leftBytes = leftExists ? leftFiles->value(path) : QByteArray{};
    const QByteArray rightBytes = rightExists ? rightFiles->value(path) : QByteArray{};
    const bool unchanged = leftExists && rightExists && leftBytes == rightBytes;

    if (unchanged) {
      ++unchangedCount;
      if (!showUnchanged) {
        continue;
      }
    } else if (!leftExists && rightExists) {
      ++addedCount;
    } else if (leftExists && !rightExists) {
      ++removedCount;
    } else {
      ++modifiedCount;
    }

    DiffEntry entry;
    entry.relativePath = path;
    entry.leftExists = leftExists;
    entry.rightExists = rightExists;
    entry.leftBytes = leftBytes;
    entry.rightBytes = rightBytes;
    entry.unchanged = unchanged;
    entry.deltaBytes = rightBytes.size() - leftBytes.size();
    entry.statusText = statusForEntry(leftExists, rightExists, unchanged, this);
    visibleEntries_.push_back(std::move(entry));
  }

  filesTable_->setRowCount(visibleEntries_.size());
  for (int row = 0; row < visibleEntries_.size(); ++row) {
    const DiffEntry& entry = visibleEntries_.at(row);
    auto* pathItem = new QTableWidgetItem(entry.relativePath);
    auto* statusItem = new QTableWidgetItem(entry.statusText);
    auto* deltaItem = new QTableWidgetItem(formatDeltaBytes(entry.deltaBytes));

    if (entry.statusText == tr("Added")) {
      statusItem->setForeground(QColor(QStringLiteral("#16a34a")));
    } else if (entry.statusText == tr("Removed")) {
      statusItem->setForeground(QColor(QStringLiteral("#dc2626")));
    } else if (entry.statusText == tr("Modified")) {
      statusItem->setForeground(QColor(QStringLiteral("#d97706")));
    } else {
      statusItem->setForeground(QColor(QStringLiteral("#6b7280")));
    }

    filesTable_->setItem(row, 0, pathItem);
    filesTable_->setItem(row, 1, statusItem);
    filesTable_->setItem(row, 2, deltaItem);
  }

  const QString summary = tr("Added: %1   Removed: %2   Modified: %3   Unchanged: %4")
                              .arg(addedCount)
                              .arg(removedCount)
                              .arg(modifiedCount)
                              .arg(unchangedCount);
  summaryLabel_->setText(summary);

  leftLabel_->setText(tr("Left: %1").arg(displayLabelForSourceId(leftId)));
  rightLabel_->setText(tr("Right: %1").arg(displayLabelForSourceId(rightId)));

  if (!visibleEntries_.isEmpty()) {
    filesTable_->selectRow(0);
    updatePreview();
  } else {
    leftPreview_->setPlainText(tr("No differences for this selection."));
    rightPreview_->setPlainText(tr("No differences for this selection."));
  }
}

void CodeSnapshotCompareDialog::updatePreview() {
  const QList<QTableWidgetItem*> selected = filesTable_->selectedItems();
  if (selected.isEmpty()) {
    leftPreview_->clear();
    rightPreview_->clear();
    return;
  }

  const int row = selected.first()->row();
  if (row < 0 || row >= visibleEntries_.size()) {
    leftPreview_->clear();
    rightPreview_->clear();
    return;
  }

  const DiffEntry& entry = visibleEntries_.at(row);
  if (entry.leftExists) {
    leftPreview_->setPlainText(decodeFileText(entry.leftBytes, this));
  } else {
    leftPreview_->setPlainText(tr("[File does not exist in left source]"));
  }
  if (entry.rightExists) {
    rightPreview_->setPlainText(decodeFileText(entry.rightBytes, this));
  } else {
    rightPreview_->setPlainText(tr("[File does not exist in right source]"));
  }
}


#include "welcome_widget.h"

#include <QBoxLayout>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QStyle>
#include <QUrl>

namespace {
constexpr int kRoleFolderPath = Qt::UserRole + 501;

QIcon iconFor(QWidget* w, const QString& themeName, QStyle::StandardPixmap fallback) {
  QIcon icon = QIcon::fromTheme(themeName);
  if (icon.isNull() && w) {
    icon = w->style()->standardIcon(fallback);
  }
  return icon;
}

}  // namespace

WelcomeWidget::WelcomeWidget(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(18, 18, 18, 18);
  layout->setSpacing(12);

  auto* title = new QLabel(tr("Welcome"), this);
  QFont f = title->font();
  f.setPointSize(std::max(12, f.pointSize() + 6));
  f.setBold(true);
  title->setFont(f);
  layout->addWidget(title);

  {
    auto* row = new QHBoxLayout();
    newSketchButton_ = new QPushButton(tr("New Sketch"), this);
    newSketchButton_->setObjectName("WelcomeNewSketchButton");
    newSketchButton_->setIcon(iconFor(this, "document-new", QStyle::SP_FileDialogNewFolder));
    openSketchButton_ = new QPushButton(tr("Open Sketch\u2026"), this);
    openSketchButton_->setObjectName("WelcomeOpenSketchButton");
    openSketchButton_->setIcon(iconFor(this, "document-open", QStyle::SP_DialogOpenButton));
    openSketchFolderButton_ = new QPushButton(tr("Open Folder\u2026"), this);
    openSketchFolderButton_->setObjectName("WelcomeOpenFolderButton");
    openSketchFolderButton_->setIcon(iconFor(this, "folder-open", QStyle::SP_DirOpenIcon));
    row->addWidget(newSketchButton_);
    row->addWidget(openSketchButton_);
    row->addWidget(openSketchFolderButton_);
    row->addStretch(1);
    layout->addLayout(row);

    connect(newSketchButton_, &QPushButton::clicked, this,
            [this] { emit newSketchRequested(); });
    connect(openSketchButton_, &QPushButton::clicked, this,
            [this] { emit openSketchRequested(); });
    connect(openSketchFolderButton_, &QPushButton::clicked, this,
            [this] { emit openSketchFolderRequested(); });
  }

  auto* listsRow = new QHBoxLayout();
  listsRow->setSpacing(12);

  {
    auto* box = new QGroupBox(tr("Pinned Sketches"), this);
    auto* v = new QVBoxLayout(box);
    pinnedList_ = new QListWidget(box);
    pinnedList_->setObjectName("WelcomePinnedList");
    pinnedList_->setSelectionMode(QAbstractItemView::SingleSelection);
    pinnedList_->setContextMenuPolicy(Qt::CustomContextMenu);
    v->addWidget(pinnedList_, 1);
    clearPinnedButton_ = new QPushButton(tr("Clear Pinned"), box);
    clearPinnedButton_->setObjectName("WelcomeClearPinnedButton");
    clearPinnedButton_->setEnabled(false);
    v->addWidget(clearPinnedButton_, 0, Qt::AlignRight);
    listsRow->addWidget(box, 1);

    connect(pinnedList_, &QListWidget::itemActivated, this,
            [this](QListWidgetItem* item) { onListActivated(item); });
    connect(pinnedList_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* item) { onListActivated(item); });
    connect(pinnedList_, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) { showPinnedContextMenu(pos); });
    connect(clearPinnedButton_, &QPushButton::clicked, this,
            [this] { emit clearPinnedRequested(); });
  }

  {
    auto* box = new QGroupBox(tr("Recent Sketches"), this);
    auto* v = new QVBoxLayout(box);
    recentList_ = new QListWidget(box);
    recentList_->setObjectName("WelcomeRecentList");
    recentList_->setSelectionMode(QAbstractItemView::SingleSelection);
    recentList_->setContextMenuPolicy(Qt::CustomContextMenu);
    v->addWidget(recentList_, 1);
    clearRecentButton_ = new QPushButton(tr("Clear Recent"), box);
    clearRecentButton_->setObjectName("WelcomeClearRecentButton");
    clearRecentButton_->setEnabled(false);
    v->addWidget(clearRecentButton_, 0, Qt::AlignRight);
    listsRow->addWidget(box, 1);

    connect(recentList_, &QListWidget::itemActivated, this,
            [this](QListWidgetItem* item) { onListActivated(item); });
    connect(recentList_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* item) { onListActivated(item); });
    connect(recentList_, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) { showRecentContextMenu(pos); });
    connect(clearRecentButton_, &QPushButton::clicked, this,
            [this] { emit clearRecentRequested(); });
  }

  layout->addLayout(listsRow, 1);

  rebuildLists();
}

void WelcomeWidget::setPinnedSketches(QStringList folders) {
  folders.removeAll(QString{});
  for (QString& s : folders) {
    s = QFileInfo(s).absoluteFilePath();
  }
  folders.removeAll(QString{});
  folders.removeDuplicates();
  pinnedFolders_ = std::move(folders);
  rebuildLists();
}

void WelcomeWidget::setRecentSketches(QStringList folders) {
  folders.removeAll(QString{});
  for (QString& s : folders) {
    s = QFileInfo(s).absoluteFilePath();
  }
  folders.removeAll(QString{});
  folders.removeDuplicates();
  recentFolders_ = std::move(folders);
  rebuildLists();
}

void WelcomeWidget::rebuildLists() {
  rebuildList(pinnedList_, pinnedFolders_, tr("(No pinned sketches)"));
  rebuildList(recentList_, recentFolders_, tr("(No recent sketches)"));
  if (clearPinnedButton_) {
    clearPinnedButton_->setEnabled(!pinnedFolders_.isEmpty());
  }
  if (clearRecentButton_) {
    clearRecentButton_->setEnabled(!recentFolders_.isEmpty());
  }
}

void WelcomeWidget::rebuildList(QListWidget* list,
                               const QStringList& folders,
                               const QString& emptyLabel) const {
  if (!list) {
    return;
  }
  list->clear();

  if (folders.isEmpty()) {
    auto* item = new QListWidgetItem(emptyLabel, list);
    item->setFlags(item->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);
    list->addItem(item);
    return;
  }

  for (const QString& folder : folders) {
    if (folder.trimmed().isEmpty()) {
      continue;
    }
    const QString label = displayNameForFolder(folder);
    auto* item = new QListWidgetItem(label, list);
    item->setData(kRoleFolderPath, folder);
    item->setToolTip(folder);
    item->setIcon(iconFor(list, "folder", QStyle::SP_DirIcon));
    list->addItem(item);
  }

  if (list->count() > 0) {
    list->setCurrentRow(0);
  }
}

void WelcomeWidget::onListActivated(QListWidgetItem* item) {
  if (!item || !(item->flags() & Qt::ItemIsEnabled)) {
    return;
  }
  const QString folder = item->data(kRoleFolderPath).toString();
  if (folder.trimmed().isEmpty()) {
    return;
  }
  emit openSketchSelected(folder);
}

void WelcomeWidget::showPinnedContextMenu(const QPoint& pos) {
  if (!pinnedList_) {
    return;
  }
  QListWidgetItem* item = pinnedList_->itemAt(pos);
  if (!item || !(item->flags() & Qt::ItemIsEnabled)) {
    return;
  }
  const QString folder = item->data(kRoleFolderPath).toString().trimmed();
  if (folder.isEmpty()) {
    return;
  }

  QMenu menu(this);
  QAction* open = menu.addAction(tr("Open"));
  QAction* unpin = menu.addAction(tr("Unpin"));
  menu.addSeparator();
  QAction* reveal = menu.addAction(tr("Show in File Manager"));

  QAction* chosen = menu.exec(pinnedList_->viewport()->mapToGlobal(pos));
  if (!chosen) {
    return;
  }
  if (chosen == open) {
    emit openSketchSelected(folder);
    return;
  }
  if (chosen == unpin) {
    emit pinSketchRequested(folder, false);
    return;
  }
  if (chosen == reveal) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    return;
  }
}

void WelcomeWidget::showRecentContextMenu(const QPoint& pos) {
  if (!recentList_) {
    return;
  }
  QListWidgetItem* item = recentList_->itemAt(pos);
  if (!item || !(item->flags() & Qt::ItemIsEnabled)) {
    return;
  }
  const QString folder = item->data(kRoleFolderPath).toString().trimmed();
  if (folder.isEmpty()) {
    return;
  }

  QMenu menu(this);
  QAction* open = menu.addAction(tr("Open"));
  QAction* pin = menu.addAction(tr("Pin"));
  QAction* remove = menu.addAction(tr("Remove from Recent"));
  menu.addSeparator();
  QAction* reveal = menu.addAction(tr("Show in File Manager"));

  QAction* chosen = menu.exec(recentList_->viewport()->mapToGlobal(pos));
  if (!chosen) {
    return;
  }
  if (chosen == open) {
    emit openSketchSelected(folder);
    return;
  }
  if (chosen == pin) {
    emit pinSketchRequested(folder, true);
    return;
  }
  if (chosen == remove) {
    emit removeRecentRequested(folder);
    return;
  }
  if (chosen == reveal) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    return;
  }
}

QString WelcomeWidget::displayNameForFolder(const QString& folder) {
  const QString name = QFileInfo(folder).fileName().trimmed();
  return name.isEmpty() ? folder : name;
}

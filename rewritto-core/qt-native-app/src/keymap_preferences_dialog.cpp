#include "keymap_preferences_dialog.h"

#include "keymap_manager.h"
#include "filter_line_edit.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QMessageBox>

namespace {
constexpr int kNameColumn = 0;
constexpr int kCategoryColumn = 1;
constexpr int kShortcutColumn = 2;
constexpr int kIdRole = Qt::UserRole;
}  // namespace

KeymapPreferencesDialog::KeymapPreferencesDialog(KeymapManager* manager, QWidget* parent)
    : QDialog(parent), manager_(manager) {
  setupUi();
  populateTree();

  connect(manager_, &KeymapManager::keybindingChanged, this, [this]() {
    populateTree();
    updateSelectionState();
  });
  connect(manager_, &KeymapManager::keybindingReset, this, [this]() {
    populateTree();
    updateSelectionState();
  });
}

KeymapPreferencesDialog::~KeymapPreferencesDialog() = default;

void KeymapPreferencesDialog::setupUi() {
  setWindowTitle(tr("Keyboard Shortcuts"));
  setMinimumSize(700, 500);

  auto* layout = new QVBoxLayout(this);

  // Filter at the top
  auto* filterLayout = new QHBoxLayout();
  filterLayout->addWidget(new QLabel(tr("Filter:")));
  filterEdit_ = new FilterLineEdit(this);
  filterEdit_->setPlaceholderText(tr("Search shortcuts..."));
  connect(filterEdit_, &FilterLineEdit::textChanged, this,
          &KeymapPreferencesDialog::onFilterTextChanged);
  filterLayout->addWidget(filterEdit_);
  layout->addLayout(filterLayout);

  // Tree widget
  tree_ = new QTreeWidget(this);
  tree_->setColumnCount(3);
  tree_->setHeaderLabels({tr("Command"), tr("Category"), tr("Shortcut")});
  tree_->setSelectionMode(QAbstractItemView::SingleSelection);
  tree_->setRootIsDecorated(false);
  tree_->setAlternatingRowColors(true);
  tree_->setSortingEnabled(true);
  tree_->sortByColumn(kNameColumn, Qt::AscendingOrder);

  connect(tree_, &QTreeWidget::itemSelectionChanged, this,
          &KeymapPreferencesDialog::onItemSelectionChanged);

  layout->addWidget(tree_);

  // Keybinding editor section
  auto* editorLayout = new QGridLayout();

  int row = 0;
  editorLayout->addWidget(new QLabel(tr("Current Shortcut:")), row, 0);
  currentKeyLabel_ = new QLabel(this);
  currentKeyLabel_->setTextStyle(Qt::Style_QFrame);
  currentKeyLabel_->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  currentKeyLabel_->setMinimumWidth(150);
  editorLayout->addWidget(currentKeyLabel_, row, 1);

  row++;
  captureButton_ = new QPushButton(tr("Capture New Shortcut"), this);
  connect(captureButton_, &QPushButton::clicked, this,
          &KeymapPreferencesDialog::onCaptureButtonClicked);
  editorLayout->addWidget(captureButton_, row, 1);

  row++;
  conflictLabel_ = new QLabel(this);
  conflictLabel_->setWordWrap(true);
  conflictLabel_->setStyleSheet("QLabel { color: red; }");
  editorLayout->addWidget(conflictLabel_, row, 0, 1, 2);

  layout->addLayout(editorLayout);

  // Status label
  statusLabel_ = new QLabel(this);
  statusLabel->setWordWrap(true);
  layout->addWidget(statusLabel_);

  // Action buttons
  auto* actionLayout = new QHBoxLayout();

  resetButton_ = new QPushButton(tr("Reset to Default"), this);
  connect(resetButton_, &QPushButton::clicked, this,
          &KeymapPreferencesDialog::onResetButtonClicked);
  actionLayout->addWidget(resetButton_);

  resetAllButton_ = new QPushButton(tr("Reset All"), this);
  connect(resetAllButton_, &QPushButton::clicked, this,
          &KeymapPreferencesDialog::onResetAllButtonClicked);
  actionLayout->addWidget(resetAllButton_);

  exportButton_ = new QPushButton(tr("Export..."), this);
  connect(exportButton_, &QPushButton::clicked, this,
          &KeymapPreferencesDialog::onExportButtonClicked);
  actionLayout->addWidget(exportButton_);

  importButton_ = new QPushButton(tr("Import..."), this);
  connect(importButton_, &QPushButton::clicked, this,
          &KeymapPreferencesDialog::onImportButtonClicked);
  actionLayout->addWidget(importButton_);

  actionLayout->addStretch();
  layout->addLayout(actionLayout);

  // Standard dialog buttons
  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Close | QDialogButtonBox::Apply, Qt::Horizontal, this);
  connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
          [this]() { manager_->saveToSettings(); });
  connect(buttons->button(QDialogButtonBox::Close), &QPushButton::clicked, this,
          &QDialog::accept);
  layout->addWidget(buttons);

  updateSelectionState();
}

void KeymapPreferencesDialog::populateTree() {
  if (!manager_) {
    return;
  }

  const QString currentFilter = filterEdit_->text();
  const QVector<KeymapEntry> allEntries = manager_->entries();

  tree_->clear();

  for (const auto& entry : allEntries) {
    // Filter by text
    if (!currentFilter.isEmpty()) {
      const QString searchText = QString("%1 %2 %3")
                                     .arg(entry.displayName, entry.category,
                                          entry.defaultSequence.toString());
      if (!searchText.contains(currentFilter, Qt::CaseInsensitive)) {
        continue;
      }
    }

    auto* item = new QTreeWidgetItem();

    const QKeySequence seq = entry.userSequence.isEmpty()
                                 ? entry.defaultSequence
                                 : entry.userSequence;

    item->setText(kNameColumn, entry.displayName);
    item->setText(kCategoryColumn, entry.category);
    item->setText(kShortcutColumn, seq.toString(QKeySequence::NativeText));
    item->setData(kShortcutColumn, kIdRole, entry.id);

    // Mark customized shortcuts
    if (!entry.userSequence.isEmpty()) {
      QFont font = item->font(kShortcutColumn);
      font.setBold(true);
      item->setFont(kShortcutColumn, font);
    }

    tree_->addTopLevelItem(item);
  }

  tree_->resizeColumnToContents(kNameColumn);
  tree_->resizeColumnToContents(kCategoryColumn);
}

void KeymapPreferencesDialog::onItemSelectionChanged() {
  updateSelectionState();
}

void KeymapPreferencesDialog::updateSelectionState() {
  const QList<QTreeWidgetItem*> selected = tree_->selectedItems();

  if (selected.isEmpty()) {
    currentEntryId_.clear();
    currentKeyLabel_->setText(QString());
    captureButton_->setEnabled(false);
    resetButton_->setEnabled(false);
    conflictLabel_->clear();
    return;
  }

  QTreeWidgetItem* item = selected.first();
  currentEntryId_ = item->data(kShortcutColumn, kIdRole).toString();

  const QVector<KeymapEntry> entries = manager_->entries();
  for (const auto& entry : entries) {
    if (entry.id == currentEntryId_) {
      const QKeySequence seq = entry.userSequence.isEmpty()
                                   ? entry.defaultSequence
                                   : entry.userSequence;
      currentKeyLabel_->setText(seq.toString(QKeySequence::NativeText));
      captureButton_->setEnabled(true);
      resetButton_->setEnabled(!entry.userSequence.isEmpty());
      break;
    }
  }

  updateConflictWarning();
}

void KeymapPreferencesDialog::updateConflictWarning(const QString& conflictId) {
  if (conflictId.isEmpty()) {
    conflictLabel_->clear();
    return;
  }

  const QVector<KeymapEntry> entries = manager_->entries();
  for (const auto& entry : entries) {
    if (entry.id == conflictId) {
      conflictLabel_->setText(
          tr("Warning: This shortcut conflicts with \"%1\" (%2)")
              .arg(entry.displayName)
              .arg(entry.category));
      return;
    }
  }

  conflictLabel_->clear();
}

void KeymapPreferencesDialog::onCaptureButtonClicked() {
  if (capturing_) {
    // Cancel capture
    capturing_ = false;
    captureButton_->setText(tr("Capture New Shortcut"));
    statusLabel_->clear();
    if (captureShortcut_) {
      captureShortcut_->setEnabled(false);
    }
    return;
  }

  capturing_ = true;
  captureButton_->setText(tr("Press Shortcut (Esc to cancel)"));
  statusLabel_->setText(tr("Press the key combination you want to use..."));
}

void KeymapPreferencesDialog::onKeySequenceCaptured(const QKeySequence& seq) {
  if (!capturing_ || currentEntryId_.isEmpty()) {
    return;
  }

  capturing_ = false;
  captureButton_->setText(tr("Capture New Shortcut"));
  statusLabel_->clear();

  if (seq.isEmpty()) {
    return;
  }

  // Check for conflicts
  const QString conflictId = manager_->findConflict(currentEntryId_, seq);
  updateConflictWarning(conflictId);

  // Ask user if they want to proceed despite conflict
  if (!conflictId.isEmpty()) {
    const QMessageBox::StandardButton result = QMessageBox::warning(
        this, tr("Shortcut Conflict"),
        tr("This shortcut conflicts with another command. Do you want to reassign it?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (result != QMessageBox::Yes) {
      return;
    }
  }

  // Set the new keybinding
  manager_->setKeybinding(currentEntryId_, seq);
  populateTree();

  // Reselect the current item
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    QTreeWidgetItem* item = tree_->topLevelItem(i);
    if (item->data(kShortcutColumn, kIdRole).toString() == currentEntryId_) {
      tree_->setCurrentItem(item);
      break;
    }
  }

  statusLabel_->setText(tr("Shortcut updated successfully."));
}

void KeymapPreferencesDialog::onResetButtonClicked() {
  if (currentEntryId_.isEmpty()) {
    return;
  }

  manager_->resetKeybinding(currentEntryId_);
  populateTree();
  updateSelectionState();
  statusLabel_->setText(tr("Shortcut reset to default."));
}

void KeymapPreferencesDialog::onResetAllButtonClicked() {
  const QMessageBox::StandardButton result =
      QMessageBox::question(this, tr("Reset All Shortcuts"),
                           tr("Are you sure you want to reset all shortcuts to their defaults?"));

  if (result == QMessageBox::Yes) {
    manager_->resetAllKeybindings();
    populateTree();
    updateSelectionState();
    statusLabel_->setText(tr("All shortcuts reset to default."));
  }
}

void KeymapPreferencesDialog::onExportButtonClicked() {
  const QString fileName = QFileDialog::getSaveFileName(
      this, tr("Export Keymap"), QString(), tr("Keymap Files (*.json);;All Files (*)"));

  if (fileName.isEmpty()) {
    return;
  }

  if (manager_->exportToFile(fileName)) {
    QMessageBox::information(this, tr("Export Successful"),
                            tr("Keymap exported successfully."));
  } else {
    QMessageBox::warning(this, tr("Export Failed"),
                        tr("Failed to export keymap to file."));
  }
}

void KeymapPreferencesDialog::onImportButtonClicked() {
  const QString fileName = QFileDialog::getOpenFileName(
      this, tr("Import Keymap"), QString(), tr("Keymap Files (*.json);;All Files (*)"));

  if (fileName.isEmpty()) {
    return;
  }

  if (manager_->importFromFile(fileName)) {
    populateTree();
    updateSelectionState();
    QMessageBox::information(this, tr("Import Successful"),
                            tr("Keymap imported successfully."));
  } else {
    QMessageBox::warning(this, tr("Import Failed"),
                        tr("Failed to import keymap from file."));
  }
}

void KeymapPreferencesDialog::onFilterTextChanged(const QString& text) {
  populateTree();
}
